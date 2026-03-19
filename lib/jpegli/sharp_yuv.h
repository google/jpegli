// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JPEGLI_SHARP_YUV_H_
#define LIB_JPEGLI_SHARP_YUV_H_

#include "lib/jpegli/encode_internal.h"

namespace jpegli {

// Applies a sharp 2x2 area downscale in linear RGB space instead of
// gamma compressed YCbCr space. This prevents bleeding/blur artifacts
// and preserves color boundaries much better.
void ApplySharpYuvDownsample(j_compress_ptr cinfo);

}  // namespace jpegli

#endif  // LIB_JPEGLI_SHARP_YUV_H_
