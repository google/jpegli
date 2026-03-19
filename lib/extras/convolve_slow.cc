// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/base/rect.h"
#include "lib/base/status.h"
#include "lib/extras/convolve-inl.h"
#include "lib/extras/convolve.h"

namespace jxl {

//------------------------------------------------------------------------------
// Slow

namespace {

// Separable kernels, any radius.
StatusOr<float> SlowSeparablePixel(const ImageF& in, const Rect& rect,
                                   const int64_t x, const int64_t y,
                                   const int64_t radius,
                                   const float* JXL_RESTRICT horz_weights,
                                   const float* JXL_RESTRICT vert_weights) {
  const size_t xsize = in.xsize();
  const size_t ysize = in.ysize();
  const WrapMirror wrap;

  float mul = 0.0f;
  for (int dy = -radius; dy <= radius; ++dy) {
    const float wy = vert_weights[std::abs(dy) * 4];
    const size_t sy = wrap(rect.y0() + y + dy, ysize);
    JXL_ENSURE(sy < ysize);
    const float* const JXL_RESTRICT row = in.ConstRow(sy);
    for (int dx = -radius; dx <= radius; ++dx) {
      const float wx = horz_weights[std::abs(dx) * 4];
      const size_t sx = wrap(rect.x0() + x + dx, xsize);
      JXL_ENSURE(sx < xsize);
      mul += row[sx] * wx * wy;
    }
  }
  return mul;
}

template <int R, typename Weights>
Status SlowSeparable(const ImageF& in, const Rect& in_rect,
                     const Weights& weights, ThreadPool* pool, ImageF* out,
                     const Rect& out_rect) {
  JXL_ENSURE(in_rect.xsize() == out_rect.xsize());
  JXL_ENSURE(in_rect.ysize() == out_rect.ysize());
  JXL_ENSURE(in_rect.IsInside(Rect(in)));
  JXL_ENSURE(out_rect.IsInside(Rect(*out)));
  const float* horz_weights = &weights.horz[0];
  const float* vert_weights = &weights.vert[0];

  const auto process_row = [&](const uint32_t task,
                               size_t /*thread*/) -> Status {
    const int64_t y = task;

    float* const JXL_RESTRICT row_out = out_rect.Row(out, y);
    for (size_t x = 0; x < in_rect.xsize(); ++x) {
      JXL_ASSIGN_OR_RETURN(row_out[x],
                           SlowSeparablePixel(in, in_rect, x, y, /*radius=*/R,
                                              horz_weights, vert_weights));
    }
    return true;
  };
  const size_t ysize = in_rect.ysize();
  JXL_RETURN_IF_ERROR(RunOnPool(pool, 0, static_cast<uint32_t>(ysize),
                                ThreadPool::NoInit, process_row,
                                "SlowSeparable"));
  return true;
}

}  // namespace

Status SlowSeparable5(const ImageF& in, const Rect& in_rect,
                      const WeightsSeparable5& weights, ThreadPool* pool,
                      ImageF* out, const Rect& out_rect) {
  return SlowSeparable<2>(in, in_rect, weights, pool, out, out_rect);
}

}  // namespace jxl
