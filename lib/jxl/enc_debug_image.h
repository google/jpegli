// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JXL_ENC_DEBUG_IMAGE_H_
#define LIB_JXL_ENC_DEBUG_IMAGE_H_

// Optional output images for debugging.

#include <cstdint>

#include "lib/jxl/base/status.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/image.h"

namespace jxl {

Status DumpImage(const CompressParams& cparams, const char* label,
                 const Image3<float>& image);
Status DumpImage(const CompressParams& cparams, const char* label,
                 const Image3<uint8_t>& image);
Status DumpXybImage(const CompressParams& cparams, const char* label,
                    const Image3<float>& image);
Status DumpPlaneNormalized(const CompressParams& cparams, const char* label,
                           const Plane<float>& image);
Status DumpPlaneNormalized(const CompressParams& cparams, const char* label,
                           const Plane<uint8_t>& image);

// Used to skip image creation if they won't be written to debug directory.
static inline bool WantDebugOutput(const CompressParams& cparams) {
  return false;
}

}  // namespace jxl

#endif  // LIB_JXL_ENC_DEBUG_IMAGE_H_
