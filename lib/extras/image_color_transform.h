// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_IMAGE_COLOR_TRANSFORM_H_
#define LIB_EXTRAS_IMAGE_COLOR_TRANSFORM_H_

#include "lib/base/data_parallel.h"
#include "lib/base/rect.h"
#include "lib/base/status.h"
#include "lib/cms/cms_interface.h"
#include "lib/cms/color_encoding_internal.h"
#include "lib/extras/image.h"

namespace jxl {

Status ApplyColorTransform(const ColorEncoding& c_current,
                           float intensity_target, const Image3F& color,
                           const ImageF* black, const Rect& rect,
                           const ColorEncoding& c_desired,
                           const JxlCmsInterface& cms, ThreadPool* pool,
                           Image3F* out);

}  // namespace jxl

#endif  // LIB_EXTRAS_IMAGE_COLOR_TRANSFORM_H_
