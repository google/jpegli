// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/jxl/dec_external_image.h"

#include <jxl/types.h>
#include <string.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "lib/jxl/base/status.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/dec_external_image.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/common.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/sanitizers.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Clamp;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::NearestInt;

// TODO(jon): check if this can be replaced by a FloatToU16 function
void FloatToU32(const float* in, uint32_t* out, size_t num, float mul,
                size_t bits_per_sample) {
  const HWY_FULL(float) d;
  const hwy::HWY_NAMESPACE::Rebind<uint32_t, decltype(d)> du;

  // Unpoison accessing partially-uninitialized vectors with memory sanitizer.
  // This is because we run NearestInt() on the vector, which triggers MSAN even
  // it is safe to do so since the values are not mixed between lanes.
  const size_t num_round_up = RoundUpTo(num, Lanes(d));
  msan::UnpoisonMemory(in + num, sizeof(in[0]) * (num_round_up - num));

  const auto one = Set(d, 1.0f);
  const auto scale = Set(d, mul);
  for (size_t x = 0; x < num; x += Lanes(d)) {
    auto v = Load(d, in + x);
    // Clamp turns NaN to 'min'.
    v = Clamp(v, Zero(d), one);
    auto i = NearestInt(Mul(v, scale));
    Store(BitCast(du, i), du, out + x);
  }

  // Poison back the output.
  msan::PoisonMemory(out + num, sizeof(out[0]) * (num_round_up - num));
}

void FloatToF16(const float* in, hwy::float16_t* out, size_t num) {
  const HWY_FULL(float) d;
  const hwy::HWY_NAMESPACE::Rebind<hwy::float16_t, decltype(d)> du;

  // Unpoison accessing partially-uninitialized vectors with memory sanitizer.
  // This is because we run DemoteTo() on the vector which triggers msan.
  const size_t num_round_up = RoundUpTo(num, Lanes(d));
  msan::UnpoisonMemory(in + num, sizeof(in[0]) * (num_round_up - num));

  for (size_t x = 0; x < num; x += Lanes(d)) {
    auto v = Load(d, in + x);
    auto v16 = DemoteTo(du, v);
    Store(v16, du, out + x);
  }

  // Poison back the output.
  msan::PoisonMemory(out + num, sizeof(out[0]) * (num_round_up - num));
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace jxl {
namespace {

// Stores a float in big endian
void StoreBEFloat(float value, uint8_t* p) {
  uint32_t u;
  memcpy(&u, &value, 4);
  StoreBE32(u, p);
}

// Stores a float in little endian
void StoreLEFloat(float value, uint8_t* p) {
  uint32_t u;
  memcpy(&u, &value, 4);
  StoreLE32(u, p);
}

}  // namespace

HWY_EXPORT(FloatToU32);
HWY_EXPORT(FloatToF16);

namespace {

using StoreFuncType = void(uint32_t value, uint8_t* dest);
template <StoreFuncType StoreFunc>
void StoreUintRow(uint32_t* JXL_RESTRICT* rows_u32, size_t num_channels,
                  size_t xsize, size_t bytes_per_sample,
                  uint8_t* JXL_RESTRICT out) {
  for (size_t x = 0; x < xsize; ++x) {
    for (size_t c = 0; c < num_channels; c++) {
      StoreFunc(rows_u32[c][x],
                out + (num_channels * x + c) * bytes_per_sample);
    }
  }
}

template <void(StoreFunc)(float, uint8_t*)>
void StoreFloatRow(const float* JXL_RESTRICT* rows_in, size_t num_channels,
                   size_t xsize, uint8_t* JXL_RESTRICT out) {
  for (size_t x = 0; x < xsize; ++x) {
    for (size_t c = 0; c < num_channels; c++) {
      StoreFunc(rows_in[c][x], out + (num_channels * x + c) * sizeof(float));
    }
  }
}

void JXL_INLINE Store8(uint32_t value, uint8_t* dest) { *dest = value & 0xff; }

}  // namespace

Status ConvertChannelsToExternal(const ImageF* in_channels[],
                                 size_t num_channels, size_t bits_per_sample,
                                 bool float_out, JxlEndianness endianness,
                                 size_t stride, jxl::ThreadPool* pool,
                                 void* out_image, size_t out_size,
                                 const PixelCallback& out_callback) {
  JXL_DASSERT(num_channels != 0 && num_channels <= kConvertMaxChannels);
  JXL_DASSERT(in_channels[0] != nullptr);
  JXL_CHECK(float_out ? bits_per_sample == 16 || bits_per_sample == 32
                      : bits_per_sample > 0 && bits_per_sample <= 16);
  const bool has_out_image = (out_image != nullptr);
  if (has_out_image == out_callback.IsPresent()) {
    return JXL_FAILURE(
        "Must provide either an out_image or an out_callback, but not both.");
  }
  std::vector<const ImageF*> channels;
  channels.assign(in_channels, in_channels + num_channels);

  const size_t bytes_per_channel = DivCeil(bits_per_sample, jxl::kBitsPerByte);
  const size_t bytes_per_pixel = num_channels * bytes_per_channel;

  std::vector<std::vector<uint8_t>> row_out_callback;
  const auto FreeCallbackOpaque = [&out_callback](void* p) {
    out_callback.destroy(p);
  };
  std::unique_ptr<void, decltype(FreeCallbackOpaque)> out_run_opaque(
      nullptr, FreeCallbackOpaque);
  auto InitOutCallback = [&](size_t num_threads) -> Status {
    if (out_callback.IsPresent()) {
      out_run_opaque.reset(out_callback.Init(num_threads, stride));
      JXL_RETURN_IF_ERROR(out_run_opaque != nullptr);
      row_out_callback.resize(num_threads);
      for (size_t i = 0; i < num_threads; ++i) {
        row_out_callback[i].resize(stride);
      }
    }
    return true;
  };

  // First channel may not be nullptr.
  size_t xsize = channels[0]->xsize();
  size_t ysize = channels[0]->ysize();
  if (stride < bytes_per_pixel * xsize) {
    return JXL_FAILURE("stride is smaller than scanline width in bytes: %" PRIuS
                       " vs %" PRIuS,
                       stride, bytes_per_pixel * xsize);
  }
  if (!out_callback.IsPresent() &&
      out_size < (ysize - 1) * stride + bytes_per_pixel * xsize) {
    return JXL_FAILURE("out_size is too small to store image");
  }

  const bool little_endian =
      endianness == JXL_LITTLE_ENDIAN ||
      (endianness == JXL_NATIVE_ENDIAN && IsLittleEndian());

  // Handle the case where a channel is nullptr by creating a single row with
  // ones to use instead.
  ImageF ones;
  for (size_t c = 0; c < num_channels; ++c) {
    if (!channels[c]) {
      JXL_ASSIGN_OR_RETURN(ones, ImageF::Create(xsize, 1));
      FillImage(1.0f, &ones);
      break;
    }
  }

  if (float_out) {
    if (bits_per_sample == 16) {
      bool swap_endianness = little_endian != IsLittleEndian();
      Plane<hwy::float16_t> f16_cache;
      JXL_RETURN_IF_ERROR(RunOnPool(
          pool, 0, static_cast<uint32_t>(ysize),
          [&](size_t num_threads) {
            StatusOr<Plane<hwy::float16_t>> f16_cache_or =
                Plane<hwy::float16_t>::Create(xsize,
                                              num_channels * num_threads);
            if (!f16_cache_or.ok()) return false;
            f16_cache = std::move(f16_cache_or).value();
            return !!InitOutCallback(num_threads);
          },
          [&](const uint32_t task, const size_t thread) {
            const int64_t y = task;
            const float* JXL_RESTRICT row_in[kConvertMaxChannels];
            for (size_t c = 0; c < num_channels; c++) {
              row_in[c] = channels[c] ? channels[c]->Row(y) : ones.Row(0);
            }
            hwy::float16_t* JXL_RESTRICT row_f16[kConvertMaxChannels];
            for (size_t c = 0; c < num_channels; c++) {
              row_f16[c] = f16_cache.Row(c + thread * num_channels);
              HWY_DYNAMIC_DISPATCH(FloatToF16)
              (row_in[c], row_f16[c], xsize);
            }
            uint8_t* row_out =
                out_callback.IsPresent()
                    ? row_out_callback[thread].data()
                    : &(reinterpret_cast<uint8_t*>(out_image))[stride * y];
            // interleave the one scanline
            hwy::float16_t* row_f16_out =
                reinterpret_cast<hwy::float16_t*>(row_out);
            for (size_t x = 0; x < xsize; x++) {
              for (size_t c = 0; c < num_channels; c++) {
                row_f16_out[x * num_channels + c] = row_f16[c][x];
              }
            }
            if (swap_endianness) {
              size_t size = xsize * num_channels * 2;
              for (size_t i = 0; i < size; i += 2) {
                std::swap(row_out[i + 0], row_out[i + 1]);
              }
            }
            if (out_callback.IsPresent()) {
              out_callback.run(out_run_opaque.get(), thread, 0, y, xsize,
                               row_out);
            }
          },
          "ConvertF16"));
    } else if (bits_per_sample == 32) {
      JXL_RETURN_IF_ERROR(RunOnPool(
          pool, 0, static_cast<uint32_t>(ysize),
          [&](size_t num_threads) { return InitOutCallback(num_threads); },
          [&](const uint32_t task, const size_t thread) {
            const int64_t y = task;
            uint8_t* row_out =
                out_callback.IsPresent()
                    ? row_out_callback[thread].data()
                    : &(reinterpret_cast<uint8_t*>(out_image))[stride * y];
            const float* JXL_RESTRICT row_in[kConvertMaxChannels];
            for (size_t c = 0; c < num_channels; c++) {
              row_in[c] = channels[c] ? channels[c]->Row(y) : ones.Row(0);
            }
            if (little_endian) {
              StoreFloatRow<StoreLEFloat>(row_in, num_channels, xsize, row_out);
            } else {
              StoreFloatRow<StoreBEFloat>(row_in, num_channels, xsize, row_out);
            }
            if (out_callback.IsPresent()) {
              out_callback.run(out_run_opaque.get(), thread, 0, y, xsize,
                               row_out);
            }
          },
          "ConvertFloat"));
    } else {
      return JXL_FAILURE("float other than 16-bit and 32-bit not supported");
    }
  } else {
    // Multiplier to convert from floating point 0-1 range to the integer
    // range.
    float mul = (1ull << bits_per_sample) - 1;
    Plane<uint32_t> u32_cache;
    JXL_RETURN_IF_ERROR(RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize),
        [&](size_t num_threads) {
          StatusOr<Plane<uint32_t>> u32_cache_or =
              Plane<uint32_t>::Create(xsize, num_channels * num_threads);
          if (!u32_cache_or.ok()) return false;
          u32_cache = std::move(u32_cache_or).value();
          return !!InitOutCallback(num_threads);
        },
        [&](const uint32_t task, const size_t thread) {
          const int64_t y = task;
          uint8_t* row_out =
              out_callback.IsPresent()
                  ? row_out_callback[thread].data()
                  : &(reinterpret_cast<uint8_t*>(out_image))[stride * y];
          const float* JXL_RESTRICT row_in[kConvertMaxChannels];
          for (size_t c = 0; c < num_channels; c++) {
            row_in[c] = channels[c] ? channels[c]->Row(y) : ones.Row(0);
          }
          uint32_t* JXL_RESTRICT row_u32[kConvertMaxChannels];
          for (size_t c = 0; c < num_channels; c++) {
            row_u32[c] = u32_cache.Row(c + thread * num_channels);
            // row_u32[] is a per-thread temporary row storage, this isn't
            // intended to be initialized on a previous run.
            msan::PoisonMemory(row_u32[c], xsize * sizeof(row_u32[c][0]));
            HWY_DYNAMIC_DISPATCH(FloatToU32)
            (row_in[c], row_u32[c], xsize, mul, bits_per_sample);
          }
          if (bits_per_sample <= 8) {
            StoreUintRow<Store8>(row_u32, num_channels, xsize, 1, row_out);
          } else {
            if (little_endian) {
              StoreUintRow<StoreLE16>(row_u32, num_channels, xsize, 2, row_out);
            } else {
              StoreUintRow<StoreBE16>(row_u32, num_channels, xsize, 2, row_out);
            }
          }
          if (out_callback.IsPresent()) {
            out_callback.run(out_run_opaque.get(), thread, 0, y, xsize,
                             row_out);
          }
        },
        "ConvertUint"));
  }
  return true;
}

}  // namespace jxl
#endif  // HWY_ONCE
