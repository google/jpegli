// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JXL_RENDER_PIPELINE_STAGE_SPOT_H_
#define LIB_JXL_RENDER_PIPELINE_STAGE_SPOT_H_

#include <utility>

#include "lib/jxl/render_pipeline/render_pipeline_stage.h"

namespace jxl {

// Render the spot color channels.
std::unique_ptr<RenderPipelineStage> GetSpotColorStage(size_t spot_c,
                                                       const float* spot_color);

}  // namespace jxl

#endif  // LIB_JXL_RENDER_PIPELINE_STAGE_SPOT_H_
