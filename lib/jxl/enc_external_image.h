// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JXL_ENC_EXTERNAL_IMAGE_H_
#define LIB_JXL_ENC_EXTERNAL_IMAGE_H_

// Interleaved image for color transforms and Codec.

#include <jxl/types.h>
#include <stddef.h>
#include <stdint.h>

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/image.h"

namespace jxl {
Status ConvertFromExternal(const uint8_t* data, size_t size, size_t xsize,
                           size_t ysize, size_t bits_per_sample,
                           JxlPixelFormat format, size_t c, ThreadPool* pool,
                           ImageF* channel);

}  // namespace jxl

#endif  // LIB_JXL_ENC_EXTERNAL_IMAGE_H_
