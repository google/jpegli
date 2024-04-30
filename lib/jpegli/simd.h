// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JPEGLI_SIMD_H_
#define LIB_JPEGLI_SIMD_H_

#include <stddef.h>

namespace jpegli {

// Returns SIMD vector size in bytes.
size_t VectorSize();

}  // namespace jpegli

#endif  // LIB_JPEGLI_SIMD_H_
