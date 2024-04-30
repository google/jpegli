// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_ENC_JPG_H_
#define LIB_EXTRAS_ENC_JPG_H_

// Encodes JPG pixels and metadata in memory.

#include <memory>

#include "lib/extras/enc/encode.h"

namespace jxl {
namespace extras {

std::unique_ptr<Encoder> GetJPEGEncoder();

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_ENC_JPG_H_
