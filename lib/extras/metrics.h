// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_METRICS_H_
#define LIB_EXTRAS_METRICS_H_

#include <stdint.h>

#include "lib/jxl/butteraugli/butteraugli.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {

// Computes the butteraugli distance and optionally the distmap of images in any
// RGB color model, optionally with alpha channel.
float ButteraugliDistance(const ImageBundle& rgb0, const ImageBundle& rgb1,
                          const ButteraugliParams& params,
                          const JxlCmsInterface& cms,
                          ImageF* distmap = nullptr,
                          ThreadPool* pool = nullptr,
                          bool ignore_alpha = false);

// Computes p-norm given the butteraugli distmap.
double ComputeDistanceP(const ImageF& distmap, const ButteraugliParams& params,
                        double p);

double ComputePSNR(const ImageBundle& ib1, const ImageBundle& ib2,
                   const JxlCmsInterface& cms);

}  // namespace jxl

#endif  // LIB_EXTRAS_METRICS_H_
