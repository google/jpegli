// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JXL_RENDER_PIPELINE_STAGE_SPLINES_H_
#define LIB_JXL_RENDER_PIPELINE_STAGE_SPLINES_H_

#include <utility>

#include "lib/jxl/render_pipeline/render_pipeline_stage.h"
#include "lib/jxl/splines.h"

namespace jxl {

// Draws splines if applicable.
std::unique_ptr<RenderPipelineStage> GetSplineStage(const Splines* splines);

}  // namespace jxl

#endif  // LIB_JXL_RENDER_PIPELINE_STAGE_SPLINES_H_
