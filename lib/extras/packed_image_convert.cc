// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/extras/packed_image_convert.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include "lib/base/byte_order.h"
#include "lib/base/common.h"
#include "lib/base/compiler_specific.h"
#include "lib/base/data_parallel.h"
#include "lib/base/float.h"
#include "lib/base/memory_manager.h"
#include "lib/base/printf_macros.h"
#include "lib/base/sanitizers.h"
#include "lib/base/status.h"
#include "lib/base/types.h"
#include "lib/cms/cms.h"
#include "lib/cms/color_encoding.h"
#include "lib/cms/color_encoding_internal.h"
#include "lib/extras/codestream_header.h"
#include "lib/extras/image.h"
#include "lib/extras/image_ops.h"
#include "lib/extras/packed_image.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/extras/packed_image_convert.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

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
namespace extras {

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

// Maximum number of channels for the ConvertChannelsToExternal function.
const size_t kConvertMaxChannels = 4;

Status ConvertChannelsToExternal(const ImageF* in_channels[],
                                 size_t num_channels, size_t bits_per_sample,
                                 bool float_out, JxlEndianness endianness,
                                 size_t stride, jxl::ThreadPool* pool,
                                 void* out_image, size_t out_size) {
  JXL_DASSERT(num_channels != 0 && num_channels <= kConvertMaxChannels);
  JXL_DASSERT(in_channels[0] != nullptr);
  JXL_ENSURE(float_out ? bits_per_sample == 16 || bits_per_sample == 32
                       : bits_per_sample > 0 && bits_per_sample <= 16);
  JXL_ENSURE(out_image);
  JxlMemoryManager* memory_manager = in_channels[0]->memory_manager();
  std::vector<const ImageF*> channels;
  channels.assign(in_channels, in_channels + num_channels);

  const size_t bytes_per_channel = DivCeil(bits_per_sample, jxl::kBitsPerByte);
  const size_t bytes_per_pixel = num_channels * bytes_per_channel;

  // First channel may not be nullptr.
  size_t xsize = channels[0]->xsize();
  size_t ysize = channels[0]->ysize();
  if (stride < bytes_per_pixel * xsize) {
    return JXL_FAILURE("stride is smaller than scanline width in bytes: %" PRIuS
                       " vs %" PRIuS,
                       stride, bytes_per_pixel * xsize);
  }
  if (out_size < (ysize - 1) * stride + bytes_per_pixel * xsize) {
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
      JXL_ASSIGN_OR_RETURN(ones, ImageF::Create(memory_manager, xsize, 1));
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
          [&](size_t num_threads) -> Status {
            JXL_ASSIGN_OR_RETURN(f16_cache, Plane<hwy::float16_t>::Create(
                                                memory_manager, xsize,
                                                num_channels * num_threads));
            return true;
          },
          [&](const uint32_t task, const size_t thread) -> Status {
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
                &(reinterpret_cast<uint8_t*>(out_image))[stride * y];
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
            return true;
          },
          "ConvertF16"));
    } else if (bits_per_sample == 32) {
      JXL_RETURN_IF_ERROR(RunOnPool(
          pool, 0, static_cast<uint32_t>(ysize),
          [&](size_t num_threads) -> Status { return true; },
          [&](const uint32_t task, const size_t thread) -> Status {
            const int64_t y = task;
            uint8_t* row_out =
                &(reinterpret_cast<uint8_t*>(out_image))[stride * y];
            const float* JXL_RESTRICT row_in[kConvertMaxChannels];
            for (size_t c = 0; c < num_channels; c++) {
              row_in[c] = channels[c] ? channels[c]->Row(y) : ones.Row(0);
            }
            if (little_endian) {
              StoreFloatRow<StoreLEFloat>(row_in, num_channels, xsize, row_out);
            } else {
              StoreFloatRow<StoreBEFloat>(row_in, num_channels, xsize, row_out);
            }
            return true;
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
        [&](size_t num_threads) -> Status {
          JXL_ASSIGN_OR_RETURN(
              u32_cache, Plane<uint32_t>::Create(memory_manager, xsize,
                                                 num_channels * num_threads));
          return true;
        },
        [&](const uint32_t task, const size_t thread) -> Status {
          const int64_t y = task;
          uint8_t* row_out =
              &(reinterpret_cast<uint8_t*>(out_image))[stride * y];
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
          return true;
        },
        "ConvertUint"));
  }
  return true;
}

namespace {

size_t JxlDataTypeBytes(JxlDataType data_type) {
  switch (data_type) {
    case JXL_TYPE_UINT8:
      return 1;
    case JXL_TYPE_UINT16:
      return 2;
    case JXL_TYPE_FLOAT16:
      return 2;
    case JXL_TYPE_FLOAT:
      return 4;
    default:
      return 0;
  }
}

}  // namespace

Status ConvertFromExternalNoSizeCheck(const uint8_t* data, size_t xsize,
                                      size_t ysize, size_t stride,
                                      size_t bits_per_sample,
                                      JxlPixelFormat format, size_t c,
                                      ThreadPool* pool, ImageF* channel) {
  if (format.data_type == JXL_TYPE_UINT8) {
    JXL_RETURN_IF_ERROR(bits_per_sample > 0 && bits_per_sample <= 8);
  } else if (format.data_type == JXL_TYPE_UINT16) {
    JXL_RETURN_IF_ERROR(bits_per_sample > 8 && bits_per_sample <= 16);
  } else if (format.data_type != JXL_TYPE_FLOAT16 &&
             format.data_type != JXL_TYPE_FLOAT) {
    return JXL_FAILURE("unsupported pixel format data type %d", format.data_type);
  }

  JXL_ENSURE(channel->xsize() == xsize);
  JXL_ENSURE(channel->ysize() == ysize);

  size_t bytes_per_channel = JxlDataTypeBytes(format.data_type);
  size_t bytes_per_pixel = format.num_channels * bytes_per_channel;
  size_t pixel_offset = c * bytes_per_channel;
  // Only for uint8/16.
  float scale = 1.0f / ((1ull << bits_per_sample) - 1);

  const bool little_endian =
      format.endianness == JXL_LITTLE_ENDIAN ||
      (format.endianness == JXL_NATIVE_ENDIAN && IsLittleEndian());

  const auto convert_row = [&](const uint32_t task,
                               size_t /*thread*/) -> Status {
    const size_t y = task;
    size_t offset = y * stride + pixel_offset;
    float* JXL_RESTRICT row_out = channel->Row(y);
    const auto save_value = [&](size_t index, float value) {
      row_out[index] = value;
    };
    if (!LoadFloatRow(data + offset, xsize, bytes_per_pixel, format.data_type,
                      little_endian, scale, save_value)) {
      return JXL_FAILURE("unsupported pixel format data type");
    }
    return true;
  };
  JXL_RETURN_IF_ERROR(RunOnPool(pool, 0, static_cast<uint32_t>(ysize),
                                ThreadPool::NoInit, convert_row,
                                "ConvertExtraChannel"));

  return true;
}

Status ConvertFromExternal(const uint8_t* data, size_t size, size_t xsize,
                           size_t ysize, size_t bits_per_sample,
                           JxlPixelFormat format, size_t c, ThreadPool* pool,
                           ImageF* channel) {
  size_t bytes_per_channel = JxlDataTypeBytes(format.data_type);
  size_t bytes_per_pixel = format.num_channels * bytes_per_channel;
  const size_t last_row_size = xsize * bytes_per_pixel;
  const size_t align = format.align;
  const size_t row_size =
      (align > 1 ? jxl::DivCeil(last_row_size, align) * align : last_row_size);
  const size_t bytes_to_read = row_size * (ysize - 1) + last_row_size;
  if (xsize == 0 || ysize == 0) return JXL_FAILURE("Empty image");
  if (size > 0 && size < bytes_to_read) {
    return JXL_FAILURE("Buffer size is too small, expected: %" PRIuS
                       " got: %" PRIuS " (Image: %" PRIuS "x%" PRIuS
                       "x%u, bytes_per_channel: %" PRIuS ")",
                       bytes_to_read, size, xsize, ysize, format.num_channels,
                       bytes_per_channel);
  }
  // Too large buffer is likely an application bug, so also fail for that.
  // Do allow padding to stride in last row though.
  if (size > row_size * ysize) {
    return JXL_FAILURE("Buffer size is too large");
  }
  return ConvertFromExternalNoSizeCheck(
      data, xsize, ysize, row_size, bits_per_sample, format, c, pool, channel);
}

Status GetColorEncoding(const PackedPixelFile& ppf,
                        ColorEncoding* color_encoding) {
  if (ppf.primary_color_representation == PackedPixelFile::kIccIsPrimary) {
    const bool is_gray = (ppf.info.num_color_channels == 1);
    IccBytes icc = ppf.icc;
    if (!color_encoding->SetICC(std::move(icc), JxlGetDefaultCms())) {
      fprintf(stderr, "Warning: error setting ICC profile, assuming SRGB\n");
      *color_encoding = ColorEncoding::SRGB(is_gray);
    } else {
      if (color_encoding->IsCMYK()) {
        // We expect gray or tri-color.
        return JXL_FAILURE("Embedded ICC is CMYK");
      }
      if (color_encoding->IsGray() != is_gray) {
        // E.g. JPG image has 3 channels, but gray ICC.
        return JXL_FAILURE("Embedded ICC does not match image color type");
      }
    }
  } else {
    JXL_RETURN_IF_ERROR(color_encoding->FromExternal(ppf.color_encoding));
    if (color_encoding->ICC().empty()) {
      return JXL_FAILURE("Failed to serialize ICC");
    }
  }
  return true;
}

float GetIntensityTarget(const extras::PackedPixelFile& ppf,
                         const ColorEncoding& c_enc) {
  if (ppf.info.intensity_target != 0) {
    return ppf.info.intensity_target;
  } else if (c_enc.Tf().IsPQ()) {
    // Peak luminance of PQ as defined by SMPTE ST 2084:2014.
    return 10000;
  } else if (c_enc.Tf().IsHLG()) {
    // Nominal display peak luminance used as a reference by
    // Rec. ITU-R BT.2100-2.
    return 1000;
  } else {
    // SDR
    return kDefaultIntensityTarget;
  }
}

Status ConvertPackedPixelFileToImage3F(const extras::PackedPixelFile& ppf,
                                       Image3F* color, ThreadPool* pool) {
  JXL_RETURN_IF_ERROR(!ppf.frames.empty());
  const extras::PackedImage& img = ppf.frames[0].color;
  size_t bits_per_sample =
      ppf.input_bitdepth.type == JXL_BIT_DEPTH_FROM_PIXEL_FORMAT
          ? extras::PackedImage::BitsPerChannel(img.format.data_type)
          : ppf.info.bits_per_sample;
  for (size_t c = 0; c < ppf.info.num_color_channels; ++c) {
    JXL_RETURN_IF_ERROR(ConvertFromExternal(
        reinterpret_cast<const uint8_t*>(img.pixels()), img.pixels_size,
        img.xsize, img.ysize, bits_per_sample, img.format, c, pool,
        &color->Plane(c)));
  }
  if (ppf.info.num_color_channels == 1) {
    JXL_RETURN_IF_ERROR(CopyImageTo(color->Plane(0), &color->Plane(1)));
    JXL_RETURN_IF_ERROR(CopyImageTo(color->Plane(0), &color->Plane(2)));
  }
  return true;
}

StatusOr<PackedPixelFile> ConvertImage3FToPackedPixelFile(
    const Image3F& image, const ColorEncoding& c_enc, JxlPixelFormat format,
    ThreadPool* pool) {
  PackedPixelFile ppf{};
  ppf.info.xsize = image.xsize();
  ppf.info.ysize = image.ysize();
  ppf.info.num_color_channels = 3;
  JXL_RETURN_IF_ERROR(PackedImage::ValidateDataType(format.data_type));
  ppf.info.bits_per_sample = PackedImage::BitsPerChannel(format.data_type);
  ppf.info.exponent_bits_per_sample = format.data_type == JXL_TYPE_FLOAT ? 8
                                      : format.data_type == JXL_TYPE_FLOAT16
                                          ? 5
                                          : 0;
  ppf.color_encoding = c_enc.ToExternal();
  ppf.frames.clear();
  JXL_ASSIGN_OR_RETURN(
      PackedFrame frame,
      PackedFrame::Create(image.xsize(), image.ysize(), format));
  const ImageF* channels[3];
  for (int c = 0; c < 3; ++c) {
    channels[c] = &image.Plane(c);
  }
  bool float_samples = ppf.info.exponent_bits_per_sample > 0;
  JXL_RETURN_IF_ERROR(ConvertChannelsToExternal(
      channels, 3, ppf.info.bits_per_sample, float_samples, format.endianness,
      frame.color.stride, pool, frame.color.pixels(0, 0, 0),
      frame.color.pixels_size));
  ppf.frames.emplace_back(std::move(frame));
  return ppf;
}

}  // namespace extras
}  // namespace jxl
#endif  // HWY_ONCE
