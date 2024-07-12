// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_PACKED_IMAGE_CONVERT_H_
#define LIB_EXTRAS_PACKED_IMAGE_CONVERT_H_

#include <jxl/types.h>

#include "lib/base/data_parallel.h"
#include "lib/base/status.h"
#include "lib/cms/color_encoding_internal.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/image.h"

namespace jxl {
namespace extras {

Status GetColorEncoding(const PackedPixelFile& ppf,
                        ColorEncoding* color_encoding);

float GetIntensityTarget(const extras::PackedPixelFile& ppf,
                         const ColorEncoding& c_enc);

Status ConvertPackedPixelFileToImage3F(const extras::PackedPixelFile& ppf,
                                       Image3F* color,
                                       ThreadPool* pool = nullptr);

PackedPixelFile ConvertImage3FToPackedPixelFile(const Image3F& image,
                                                const ColorEncoding& c_enc,
                                                JxlPixelFormat format,
                                                ThreadPool* pool);
}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_PACKED_IMAGE_CONVERT_H_
