// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JXL_ENC_XYB_H_
#define LIB_JXL_ENC_XYB_H_

// Converts to XYB color space.

#include <jxl/cms_interface.h>

#include <cstddef>

#include "lib/jxl/base/compiler_specific.h"

namespace jxl {

void LinearRGBRowToXYB(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                       float* JXL_RESTRICT row2,
                       const float* JXL_RESTRICT premul_absorb, size_t xsize);

void ComputePremulAbsorb(float intensity_target, float* premul_absorb);

// Transforms each color component of the given XYB row into the [0.0, 1.0]
// interval with an affine transform.
void ScaleXYBRow(float* row0, float* row1, float* row2, size_t xsize);

}  // namespace jxl

#endif  // LIB_JXL_ENC_XYB_H_
