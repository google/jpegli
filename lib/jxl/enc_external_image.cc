// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/jxl/enc_external_image.h"

#include <jxl/types.h>
#include <string.h>

#include <atomic>
#include <utility>

#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/common.h"
#include "lib/jxl/base/float.h"
#include "lib/jxl/base/printf_macros.h"

namespace jxl {
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
    JXL_FAILURE("unsupported pixel format data type %d", format.data_type);
  }

  JXL_ASSERT(channel->xsize() == xsize);
  JXL_ASSERT(channel->ysize() == ysize);

  size_t bytes_per_channel = JxlDataTypeBytes(format.data_type);
  size_t bytes_per_pixel = format.num_channels * bytes_per_channel;
  size_t pixel_offset = c * bytes_per_channel;
  // Only for uint8/16.
  float scale = 1.0f;
  if (format.data_type == JXL_TYPE_UINT8) {
    // We will do an integer multiplication by 257 in LoadFloatRow so that a
    // UINT8 value and the corresponding UINT16 value convert to the same float
    scale = 1.0f / (257 * ((1ull << bits_per_sample) - 1));
  } else {
    scale = 1.0f / ((1ull << bits_per_sample) - 1);
  }

  const bool little_endian =
      format.endianness == JXL_LITTLE_ENDIAN ||
      (format.endianness == JXL_NATIVE_ENDIAN && IsLittleEndian());

  std::atomic<size_t> error_count = {0};

  const auto convert_row = [&](const uint32_t task, size_t /*thread*/) {
    const size_t y = task;
    size_t offset = y * stride + pixel_offset;
    float* JXL_RESTRICT row_out = channel->Row(y);
    const auto save_value = [&](size_t index, float value) {
      row_out[index] = value;
    };
    if (!LoadFloatRow(data + offset, xsize, bytes_per_pixel, format.data_type,
                      little_endian, scale, save_value)) {
      error_count++;
    }
  };
  JXL_RETURN_IF_ERROR(RunOnPool(pool, 0, static_cast<uint32_t>(ysize),
                                ThreadPool::NoInit, convert_row,
                                "ConvertExtraChannel"));

  if (error_count) {
    JXL_FAILURE("unsupported pixel format data type");
  }

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
}  // namespace jxl
