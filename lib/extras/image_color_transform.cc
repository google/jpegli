// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/extras/image_color_transform.h"

#include <utility>

#include "lib/base/memory_manager.h"
#include "lib/base/rect.h"
#include "lib/base/status.h"
#include "lib/cms/cms_interface.h"
#include "lib/cms/color_encoding_internal.h"

namespace jxl {

Status ApplyColorTransform(const ColorEncoding& c_current,
                           float intensity_target, const Image3F& color,
                           const ImageF* black, const Rect& rect,
                           const ColorEncoding& c_desired,
                           const JxlCmsInterface& cms, ThreadPool* pool,
                           Image3F* out) {
  ColorSpaceTransform c_transform(cms);
  // Changing IsGray is probably a bug.
  JXL_CHECK(c_current.IsGray() == c_desired.IsGray());
  bool is_gray = c_current.IsGray();
  JxlMemoryManager* memory_amanger = color.memory_manager();
  if (out->xsize() < rect.xsize() || out->ysize() < rect.ysize()) {
    JXL_ASSIGN_OR_RETURN(
        *out, Image3F::Create(memory_amanger, rect.xsize(), rect.ysize()));
  } else {
    out->ShrinkTo(rect.xsize(), rect.ysize());
  }
  const auto init = [&](const size_t num_threads) -> Status {
    JXL_RETURN_IF_ERROR(c_transform.Init(c_current, c_desired, intensity_target,
                                         rect.xsize(), num_threads));
    return true;
  };
  const auto transform_row = [&](const uint32_t y,
                                 const size_t thread) -> Status {
    float* mutable_src_buf = c_transform.BufSrc(thread);
    const float* src_buf = mutable_src_buf;
    // Interleave input.
    if (is_gray) {
      src_buf = rect.ConstPlaneRow(color, 0, y);
    } else if (c_current.IsCMYK()) {
      if (!black)
        return JXL_FAILURE("Black plane is missing for CMYK transform");
      const float* JXL_RESTRICT row_in0 = rect.ConstPlaneRow(color, 0, y);
      const float* JXL_RESTRICT row_in1 = rect.ConstPlaneRow(color, 1, y);
      const float* JXL_RESTRICT row_in2 = rect.ConstPlaneRow(color, 2, y);
      const float* JXL_RESTRICT row_in3 = rect.ConstRow(*black, y);
      for (size_t x = 0; x < rect.xsize(); x++) {
        // CMYK convention in JXL: 0 = max ink, 1 = white
        mutable_src_buf[4 * x + 0] = row_in0[x];
        mutable_src_buf[4 * x + 1] = row_in1[x];
        mutable_src_buf[4 * x + 2] = row_in2[x];
        mutable_src_buf[4 * x + 3] = row_in3[x];
      }
    } else {
      const float* JXL_RESTRICT row_in0 = rect.ConstPlaneRow(color, 0, y);
      const float* JXL_RESTRICT row_in1 = rect.ConstPlaneRow(color, 1, y);
      const float* JXL_RESTRICT row_in2 = rect.ConstPlaneRow(color, 2, y);
      for (size_t x = 0; x < rect.xsize(); x++) {
        mutable_src_buf[3 * x + 0] = row_in0[x];
        mutable_src_buf[3 * x + 1] = row_in1[x];
        mutable_src_buf[3 * x + 2] = row_in2[x];
      }
    }
    float* JXL_RESTRICT dst_buf = c_transform.BufDst(thread);
    JXL_RETURN_IF_ERROR(
        c_transform.Run(thread, src_buf, dst_buf, rect.xsize()));
    float* JXL_RESTRICT row_out0 = out->PlaneRow(0, y);
    float* JXL_RESTRICT row_out1 = out->PlaneRow(1, y);
    float* JXL_RESTRICT row_out2 = out->PlaneRow(2, y);
    // De-interleave output and convert type.
    if (is_gray) {
      for (size_t x = 0; x < rect.xsize(); x++) {
        row_out0[x] = dst_buf[x];
        row_out1[x] = dst_buf[x];
        row_out2[x] = dst_buf[x];
      }
    } else {
      for (size_t x = 0; x < rect.xsize(); x++) {
        row_out0[x] = dst_buf[3 * x + 0];
        row_out1[x] = dst_buf[3 * x + 1];
        row_out2[x] = dst_buf[3 * x + 2];
      }
    }
    return true;
  };
  JXL_RETURN_IF_ERROR(RunOnPool(pool, 0, rect.ysize(), init, transform_row,
                                "Colorspace transform"));
  return true;
}

}  // namespace jxl
