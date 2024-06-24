// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_CONVOLVE_H_
#define LIB_EXTRAS_CONVOLVE_H_

// 2D convolution.

#include <stddef.h>

#include "lib/base/data_parallel.h"
#include "lib/base/rect.h"
#include "lib/extras/image.h"

namespace jxl {

// No valid values outside [0, xsize), but the strategy may still safely load
// the preceding vector, and/or round xsize up to the vector lane count. This
// avoids needing PadImage.
// Requires xsize >= kConvolveLanes + kConvolveMaxRadius.
static constexpr size_t kConvolveMaxRadius = 3;

// Weights for separable 5x5 filters (typically but not necessarily the same
// values for horizontal and vertical directions). The kernel must already be
// normalized, but note that values for negative offsets are omitted, so the
// given values do not sum to 1.
struct WeightsSeparable5 {
  // Horizontal 1D, distances 0..2 (each replicated 4x)
  float horz[3 * 4];
  float vert[3 * 4];
};

const WeightsSeparable5& WeightsSeparable5Lowpass();

Status SlowSeparable5(const ImageF& in, const Rect& in_rect,
                      const WeightsSeparable5& weights, ThreadPool* pool,
                      ImageF* out, const Rect& out_rect);

Status Separable5(const ImageF& in, const Rect& rect,
                  const WeightsSeparable5& weights, ThreadPool* pool,
                  ImageF* out);

}  // namespace jxl

#endif  // LIB_EXTRAS_CONVOLVE_H_
