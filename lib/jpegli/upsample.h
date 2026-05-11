// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_LIB_JPEGLI_UPSAMPLE_H_
#define JPEGLI_LIB_JPEGLI_UPSAMPLE_H_

#include <stddef.h>

#include "lib/base/compiler_specific.h"

namespace jpegli {

void Upsample2Horizontal(float* JPEGLI_RESTRICT row,
                         float* JPEGLI_RESTRICT scratch_space, size_t len_out);

void Upsample2Vertical(const float* JPEGLI_RESTRICT row_top,
                       const float* JPEGLI_RESTRICT row_mid,
                       const float* JPEGLI_RESTRICT row_bot,
                       float* JPEGLI_RESTRICT row_out0,
                       float* JPEGLI_RESTRICT row_out1, size_t len);

}  // namespace jpegli

#endif  // JPEGLI_LIB_JPEGLI_UPSAMPLE_H_
