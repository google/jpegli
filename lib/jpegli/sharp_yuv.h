// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JPEGLI_SHARP_YUV_H_
#define LIB_JPEGLI_SHARP_YUV_H_

#include "lib/jpegli/encode_internal.h"

namespace jpegli {

// Applies an iterative sharp chroma downsampling for 4:2:0 subsampling.
// The algorithm downsamples Cb/Cr in linear RGB space and iteratively refines
// the chroma values to minimize luma error after upsample, preserving color
// boundaries better than a simple box filter.
void ApplySharpYuvDownsample(j_compress_ptr cinfo);

}  // namespace jpegli

#endif  // LIB_JPEGLI_SHARP_YUV_H_
