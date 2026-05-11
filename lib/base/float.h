// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_LIB_BASE_FLOAT_H_
#define JPEGLI_LIB_BASE_FLOAT_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lib/base/byte_order.h"
#include "lib/base/compiler_specific.h"
#include "lib/base/status.h"
#include "lib/base/types.h"

namespace jpegli {

namespace detail {
// Based on highway scalar implementation, for testing
static JPEGLI_INLINE float LoadFloat16(uint16_t bits16) {
  const uint32_t sign = bits16 >> 15;
  const uint32_t biased_exp = (bits16 >> 10) & 0x1F;
  const uint32_t mantissa = bits16 & 0x3FF;

  // Subnormal or zero
  if (biased_exp == 0) {
    const float subnormal =
        (1.0f / 16384) * (static_cast<float>(mantissa) * (1.0f / 1024));
    return sign ? -subnormal : subnormal;
  }

  // Normalized: convert the representation directly (faster than ldexp/tables).
  const uint32_t biased_exp32 = biased_exp + (127 - 15);
  const uint32_t mantissa32 = mantissa << (23 - 10);
  const uint32_t bits32 = (sign << 31) | (biased_exp32 << 23) | mantissa32;

  float result;
  memcpy(&result, &bits32, 4);
  return result;
}
}  // namespace detail

template <typename SaveFloatAtFn>
static Status JPEGLI_INLINE LoadFloatRow(const uint8_t* src, size_t count,
                                         size_t stride, JpegliDataType type,
                                         bool little_endian, float scale,
                                         SaveFloatAtFn callback) {
  switch (type) {
    case JPEGLI_TYPE_FLOAT:
      if (little_endian) {
        for (size_t i = 0; i < count; ++i) {
          callback(i, LoadLEFloat(src + stride * i));
        }
      } else {
        for (size_t i = 0; i < count; ++i) {
          callback(i, LoadBEFloat(src + stride * i));
        }
      }
      return true;

    case JPEGLI_TYPE_UINT8:
      for (size_t i = 0; i < count; ++i) {
        callback(i, src[stride * i] * scale);
      }
      return true;

    case JPEGLI_TYPE_UINT16:
      if (little_endian) {
        for (size_t i = 0; i < count; ++i) {
          callback(i, LoadLE16(src + stride * i) * scale);
        }
      } else {
        for (size_t i = 0; i < count; ++i) {
          callback(i, LoadBE16(src + stride * i) * scale);
        }
      }
      return true;

    case JPEGLI_TYPE_FLOAT16:
      if (little_endian) {
        for (size_t i = 0; i < count; ++i) {
          callback(i, detail::LoadFloat16(LoadLE16(src + stride * i)));
        }
      } else {
        for (size_t i = 0; i < count; ++i) {
          callback(i, detail::LoadFloat16(LoadBE16(src + stride * i)));
        }
      }
      return true;

    default:
      return JPEGLI_FAILURE("Unsupported sample format");
  }
}

}  // namespace jpegli

#endif  // JPEGLI_LIB_BASE_FLOAT_H_
