// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_LIB_JPEGLI_SIMD_H_
#define JPEGLI_LIB_JPEGLI_SIMD_H_

#include <stddef.h>

namespace jpegli {

// Returns SIMD vector size in bytes.
size_t VectorSize();

}  // namespace jpegli

#endif  // JPEGLI_LIB_JPEGLI_SIMD_H_
