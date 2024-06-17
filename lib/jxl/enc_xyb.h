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
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/image.h"

namespace jxl {

// Converts any color space to XYB in-place. If `linear` is not null, fills it
// with a linear sRGB copy of `image`.
void ToXYB(const ColorEncoding& c_current, float intensity_target,
           const ImageF* black, ThreadPool* pool, Image3F* JXL_RESTRICT image,
           const JxlCmsInterface& cms, Image3F* JXL_RESTRICT linear);

void LinearRGBRowToXYB(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                       float* JXL_RESTRICT row2,
                       const float* JXL_RESTRICT premul_absorb, size_t xsize);

void ComputePremulAbsorb(float intensity_target, float* premul_absorb);

// Transforms each color component of the given XYB image into the [0.0, 1.0]
// interval with an affine transform.
void ScaleXYB(Image3F* opsin);
void ScaleXYBRow(float* row0, float* row1, float* row2, size_t xsize);

}  // namespace jxl

#endif  // LIB_JXL_ENC_XYB_H_
