// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_DEC_PNM_H_
#define LIB_EXTRAS_DEC_PNM_H_

// Decodes PBM/PGM/PPM/PFM pixels in memory.

#include <cstddef>
#include <cstdint>

#include "lib/extras/codestream_header.h"
// TODO(janwas): workaround for incorrect Win64 codegen (cause unknown)
#include <vector>

#include "lib/base/span.h"
#include "lib/base/status.h"
#include "lib/extras/dec/color_hints.h"
#include "lib/extras/packed_image.h"

namespace jxl {

struct SizeConstraints;

namespace extras {

// Decodes `bytes` into `ppf`. color_hints may specify "color_space", which
// defaults to sRGB.
Status DecodeImagePNM(Span<const uint8_t> bytes, const ColorHints& color_hints,
                      PackedPixelFile* ppf,
                      const SizeConstraints* constraints = nullptr);

struct HeaderPNM {
  size_t xsize;
  size_t ysize;
  bool is_gray;    // PGM
  bool has_alpha;  // PAM
  size_t bits_per_sample;
  bool floating_point;
  bool big_endian;
  std::vector<JxlExtraChannelType> ec_types;  // PAM
};

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_DEC_PNM_H_
