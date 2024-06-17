// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_PACKED_IMAGE_CONVERT_H_
#define LIB_EXTRAS_PACKED_IMAGE_CONVERT_H_

// Helper functions to convert from the external image types to the internal
// CodecInOut to help transitioning to the external types.

#include <jxl/types.h>

#include "lib/extras/packed_image.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"

namespace jxl {
namespace extras {

Status GetColorEncoding(const PackedPixelFile& ppf,
                        ColorEncoding* color_encoding);

// Converts an external PackedPixelFile to the internal CodecInOut for use with
// internal functions directly.
Status ConvertPackedPixelFileToCodecInOut(const PackedPixelFile& ppf,
                                          ThreadPool* pool, CodecInOut* io);

PackedPixelFile ConvertImage3FToPackedPixelFile(const Image3F& image,
                                                const ColorEncoding& c_enc,
                                                JxlPixelFormat format,
                                                ThreadPool* pool);
}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_PACKED_IMAGE_CONVERT_H_
