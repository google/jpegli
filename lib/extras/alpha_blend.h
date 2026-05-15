// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_LIB_EXTRAS_ALPHA_BLEND_H_
#define JPEGLI_LIB_EXTRAS_ALPHA_BLEND_H_

#include "lib/base/status.h"
#include "lib/extras/packed_image.h"

namespace jpegli {
namespace extras {

Status AlphaBlend(PackedPixelFile* ppf, const float background[3]);

}  // namespace extras
}  // namespace jpegli

#endif  // JPEGLI_LIB_EXTRAS_ALPHA_BLEND_H_
