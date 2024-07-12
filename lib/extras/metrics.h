// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_METRICS_H_
#define LIB_EXTRAS_METRICS_H_

#include <stdint.h>

#include "lib/base/data_parallel.h"
#include "lib/cms/cms_interface.h"
#include "lib/extras/butteraugli.h"
#include "lib/extras/packed_image.h"
#include "lib/extras/image.h"

namespace jxl {

// Computes the butteraugli distance and optionally the distmap of images in any
// RGB color model, optionally with alpha channel.
float ButteraugliDistance(const extras::PackedPixelFile& a,
                          const extras::PackedPixelFile& b,
                          ButteraugliParams params = ButteraugliParams(),
                          ImageF* distmap = nullptr, ThreadPool* pool = nullptr,
                          bool ignore_alpha = false);

// Computes p-norm given the butteraugli distmap.
double ComputeDistanceP(const ImageF& distmap, const ButteraugliParams& params,
                        double p);

float Butteraugli3Norm(const extras::PackedPixelFile& a,
                       const extras::PackedPixelFile& b,
                       ThreadPool* pool = nullptr);

double ComputePSNR(const extras::PackedPixelFile& a,
                   const extras::PackedPixelFile& b,
                   const JxlCmsInterface& cms);

}  // namespace jxl

#endif  // LIB_EXTRAS_METRICS_H_
