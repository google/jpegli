// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/jxl/base/rect.h"
#include "lib/extras/convolve-inl.h"
#include "lib/extras/convolve.h"

namespace jxl {

//------------------------------------------------------------------------------
// Kernels

// 4 instances of a given literal value, useful as input to LoadDup128.
#define JXL_REP4(literal) literal, literal, literal, literal

const WeightsSeparable5& WeightsSeparable5Lowpass() {
  constexpr float w0 = 0.41714928f;
  constexpr float w1 = 0.25539268f;
  constexpr float w2 = 0.03603267f;
  static constexpr WeightsSeparable5 weights = {
      {JXL_REP4(w0), JXL_REP4(w1), JXL_REP4(w2)},
      {JXL_REP4(w0), JXL_REP4(w1), JXL_REP4(w2)}};
  return weights;
}

const WeightsSeparable5& WeightsSeparable5Gaussian1() {
  constexpr float w0 = 0.38774f;
  constexpr float w1 = 0.24477f;
  constexpr float w2 = 0.06136f;
  static constexpr WeightsSeparable5 weights = {
      {JXL_REP4(w0), JXL_REP4(w1), JXL_REP4(w2)},
      {JXL_REP4(w0), JXL_REP4(w1), JXL_REP4(w2)}};
  return weights;
}

const WeightsSeparable5& WeightsSeparable5Gaussian2() {
  constexpr float w0 = 0.250301f;
  constexpr float w1 = 0.221461f;
  constexpr float w2 = 0.153388f;
  static constexpr WeightsSeparable5 weights = {
      {JXL_REP4(w0), JXL_REP4(w1), JXL_REP4(w2)},
      {JXL_REP4(w0), JXL_REP4(w1), JXL_REP4(w2)}};
  return weights;
}

#undef JXL_REP4

//------------------------------------------------------------------------------
// Slow

namespace {

// Separable kernels, any radius.
float SlowSeparablePixel(const ImageF& in, const Rect& rect, const int64_t x,
                         const int64_t y, const int64_t radius,
                         const float* JXL_RESTRICT horz_weights,
                         const float* JXL_RESTRICT vert_weights) {
  const size_t xsize = in.xsize();
  const size_t ysize = in.ysize();
  const WrapMirror wrap;

  float mul = 0.0f;
  for (int dy = -radius; dy <= radius; ++dy) {
    const float wy = vert_weights[std::abs(dy) * 4];
    const size_t sy = wrap(rect.y0() + y + dy, ysize);
    JXL_CHECK(sy < ysize);
    const float* const JXL_RESTRICT row = in.ConstRow(sy);
    for (int dx = -radius; dx <= radius; ++dx) {
      const float wx = horz_weights[std::abs(dx) * 4];
      const size_t sx = wrap(rect.x0() + x + dx, xsize);
      JXL_CHECK(sx < xsize);
      mul += row[sx] * wx * wy;
    }
  }
  return mul;
}

template <int R, typename Weights>
void SlowSeparable(const ImageF& in, const Rect& in_rect,
                   const Weights& weights, ThreadPool* pool, ImageF* out,
                   const Rect& out_rect) {
  JXL_ASSERT(in_rect.xsize() == out_rect.xsize());
  JXL_ASSERT(in_rect.ysize() == out_rect.ysize());
  JXL_ASSERT(in_rect.IsInside(Rect(in)));
  JXL_ASSERT(out_rect.IsInside(Rect(*out)));
  const float* horz_weights = &weights.horz[0];
  const float* vert_weights = &weights.vert[0];

  const size_t ysize = in_rect.ysize();
  JXL_CHECK(RunOnPool(
      pool, 0, static_cast<uint32_t>(ysize), ThreadPool::NoInit,
      [&](const uint32_t task, size_t /*thread*/) {
        const int64_t y = task;

        float* const JXL_RESTRICT row_out = out_rect.Row(out, y);
        for (size_t x = 0; x < in_rect.xsize(); ++x) {
          row_out[x] = SlowSeparablePixel(in, in_rect, x, y, /*radius=*/R,
                                          horz_weights, vert_weights);
        }
      },
      "SlowSeparable"));
}

}  // namespace

void SlowSeparable5(const ImageF& in, const Rect& in_rect,
                    const WeightsSeparable5& weights, ThreadPool* pool,
                    ImageF* out, const Rect& out_rect) {
  SlowSeparable<2>(in, in_rect, weights, pool, out, out_rect);
}

}  // namespace jxl
