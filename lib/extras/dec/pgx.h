// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_DEC_PGX_H_
#define LIB_EXTRAS_DEC_PGX_H_

// Decodes PGX pixels in memory.

#include <cstdint>

#include "lib/base/span.h"
#include "lib/base/status.h"

namespace jxl {

struct SizeConstraints;

namespace extras {

class ColorHints;
class PackedPixelFile;

// Decodes `bytes` into `ppf`.
Status DecodeImagePGX(Span<const uint8_t> bytes, const ColorHints& color_hints,
                      PackedPixelFile* ppf,
                      const SizeConstraints* constraints = nullptr);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_DEC_PGX_H_
