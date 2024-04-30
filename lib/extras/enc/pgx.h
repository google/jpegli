// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_ENC_PGX_H_
#define LIB_EXTRAS_ENC_PGX_H_

// Encodes PGX pixels in memory.

#include <stddef.h>
#include <stdint.h>

#include "lib/extras/enc/encode.h"

namespace jxl {
namespace extras {

std::unique_ptr<Encoder> GetPGXEncoder();

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_ENC_PGX_H_
