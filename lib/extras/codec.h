// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_CODEC_H_
#define LIB_EXTRAS_CODEC_H_

// Facade for image encoders/decoders (PNG, PNM, ...).

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "lib/extras/dec/color_hints.h"
#include "lib/extras/dec/decode.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"

namespace jxl {

struct SizeConstraints;

Status Encode(const extras::PackedPixelFile& ppf, extras::Codec codec,
              std::vector<uint8_t>* bytes, ThreadPool* pool);

Status Encode(const extras::PackedPixelFile& ppf, const std::string& pathname,
              std::vector<uint8_t>* bytes, ThreadPool* pool = nullptr);

}  // namespace jxl

#endif  // LIB_EXTRAS_CODEC_H_
