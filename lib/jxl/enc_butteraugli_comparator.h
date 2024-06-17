// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JXL_ENC_BUTTERAUGLI_COMPARATOR_H_
#define LIB_JXL_ENC_BUTTERAUGLI_COMPARATOR_H_

#include <jxl/cms_interface.h>
#include <stddef.h>

#include "lib/jxl/base/status.h"
#include "lib/jxl/butteraugli/butteraugli.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {

// Computes the butteraugli distance and optionally the diffmap of images in any
// RGB color model, optionally with alpha channel.
float ButteraugliDistance(const ImageBundle& rgb0, const ImageBundle& rgb1,
                          const ButteraugliParams& params,
                          const JxlCmsInterface& cms,
                          ImageF* diffmap = nullptr,
                          ThreadPool* pool = nullptr,
                          bool ignore_alpha = false);

}  // namespace jxl

#endif  // LIB_JXL_ENC_BUTTERAUGLI_COMPARATOR_H_
