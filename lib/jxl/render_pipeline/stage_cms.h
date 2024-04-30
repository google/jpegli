// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JXL_RENDER_PIPELINE_STAGE_CMS_H_
#define LIB_JXL_RENDER_PIPELINE_STAGE_CMS_H_

#include <memory>

#include "lib/jxl/dec_xyb.h"
#include "lib/jxl/render_pipeline/render_pipeline_stage.h"

namespace jxl {

std::unique_ptr<RenderPipelineStage> GetCmsStage(
    const OutputEncodingInfo& output_encoding_info);

}  // namespace jxl

#endif  // LIB_JXL_RENDER_PIPELINE_STAGE_CMS_H_
