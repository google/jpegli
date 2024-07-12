// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_DEC_EXR_H_
#define LIB_EXTRAS_DEC_EXR_H_

// Decodes OpenEXR images in memory.

#include "lib/base/data_parallel.h"
#include "lib/base/span.h"
#include "lib/base/status.h"
#include "lib/extras/dec/color_hints.h"
#include "lib/extras/packed_image.h"

namespace jxl {

struct SizeConstraints;

namespace extras {

bool CanDecodeEXR();

// Decodes `bytes` into `ppf`. color_hints are ignored.
Status DecodeImageEXR(Span<const uint8_t> bytes, const ColorHints& color_hints,
                      PackedPixelFile* ppf,
                      const SizeConstraints* constraints = nullptr);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_DEC_EXR_H_
