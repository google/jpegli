// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JXL_SIMD_UTIL_H_
#define LIB_JXL_SIMD_UTIL_H_
#include <stddef.h>

namespace jxl {

// Maximal vector size in bytes.
size_t MaxVectorSize();

// Returns distance [bytes] between the start of two consecutive rows, a
// multiple of vector/cache line size but NOT CacheAligned::kAlias - see below.
size_t BytesPerRow(size_t xsize, size_t sizeof_t);

}  // namespace jxl

#endif  // LIB_JXL_SIMD_UTIL_H_
