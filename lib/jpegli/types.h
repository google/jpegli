// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_LIB_JPEGLI_TYPES_H_
#define JPEGLI_LIB_JPEGLI_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

//
// New API structs and functions that are not available in libjpeg
//
// NOTE: This part of the API is still experimental and will probably change in
// the future.
//

/** Data type for the sample values per channel per pixel.
 */
typedef enum {
  /** Use 32-bit single-precision floating point values, with range 0.0-1.0
   * (within gamut, may go outside this range for wide color gamut). Floating
   * point output, either ::JPEGLI_TYPE_FLOAT or ::JPEGLI_TYPE_FLOAT16, is
   * recommended for HDR and wide gamut images when color profile conversion is
   * required. */
  JPEGLI_TYPE_FLOAT = 0,

  /** Use type uint8_t. May clip wide color gamut data.
   */
  JPEGLI_TYPE_UINT8 = 2,

  /** Use type uint16_t. May clip wide color gamut data.
   */
  JPEGLI_TYPE_UINT16 = 3,

  /** Use 16-bit IEEE 754 half-precision floating point values */
  JPEGLI_TYPE_FLOAT16 = 5,
} JpegliDataType;

/** Ordering of multi-byte data.
 */
typedef enum {
  /** Use the endianness of the system, either little endian or big endian,
   * without forcing either specific endianness. Do not use if pixel data
   * should be exported to a well defined format.
   */
  JPEGLI_NATIVE_ENDIAN = 0,
  /** Force little endian */
  JPEGLI_LITTLE_ENDIAN = 1,
  /** Force big endian */
  JPEGLI_BIG_ENDIAN = 2,
} JpegliEndianness;

int jpegli_bytes_per_sample(JpegliDataType data_type);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // JPEGLI_LIB_JPEGLI_TYPES_H_
