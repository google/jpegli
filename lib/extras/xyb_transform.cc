// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/extras/xyb_transform.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/extras/xyb_transform.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/base/compiler_specific.h"
#include "lib/base/fast_math-inl.h"
#include "lib/cms/opsin_params.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Add;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::MulAdd;
using hwy::HWY_NAMESPACE::Sub;
using hwy::HWY_NAMESPACE::ZeroIfNegative;

// 4x3 matrix * 3x1 SIMD vectors
template <class V>
JXL_INLINE void OpsinAbsorbance(const V r, const V g, const V b,
                                const float* JXL_RESTRICT premul_absorb,
                                V* JXL_RESTRICT mixed0, V* JXL_RESTRICT mixed1,
                                V* JXL_RESTRICT mixed2) {
  const float* bias = jxl::cms::kOpsinAbsorbanceBias.data();
  const HWY_FULL(float) d;
  const size_t N = Lanes(d);
  const auto m0 = Load(d, premul_absorb + 0 * N);
  const auto m1 = Load(d, premul_absorb + 1 * N);
  const auto m2 = Load(d, premul_absorb + 2 * N);
  const auto m3 = Load(d, premul_absorb + 3 * N);
  const auto m4 = Load(d, premul_absorb + 4 * N);
  const auto m5 = Load(d, premul_absorb + 5 * N);
  const auto m6 = Load(d, premul_absorb + 6 * N);
  const auto m7 = Load(d, premul_absorb + 7 * N);
  const auto m8 = Load(d, premul_absorb + 8 * N);
  *mixed0 = MulAdd(m0, r, MulAdd(m1, g, MulAdd(m2, b, Set(d, bias[0]))));
  *mixed1 = MulAdd(m3, r, MulAdd(m4, g, MulAdd(m5, b, Set(d, bias[1]))));
  *mixed2 = MulAdd(m6, r, MulAdd(m7, g, MulAdd(m8, b, Set(d, bias[2]))));
}

template <class V>
void StoreXYB(const V r, V g, const V b, float* JXL_RESTRICT valx,
              float* JXL_RESTRICT valy, float* JXL_RESTRICT valz) {
  const HWY_FULL(float) d;
  const V half = Set(d, 0.5f);
  Store(Mul(half, Sub(r, g)), d, valx);
  Store(Mul(half, Add(r, g)), d, valy);
  Store(b, d, valz);
}

// Converts one RGB vector to XYB.
template <class V>
void LinearRGBToXYB(const V r, const V g, const V b,
                    const float* JXL_RESTRICT premul_absorb,
                    float* JXL_RESTRICT valx, float* JXL_RESTRICT valy,
                    float* JXL_RESTRICT valz) {
  V mixed0;
  V mixed1;
  V mixed2;
  OpsinAbsorbance(r, g, b, premul_absorb, &mixed0, &mixed1, &mixed2);

  // mixed* should be non-negative even for wide-gamut, so clamp to zero.
  mixed0 = ZeroIfNegative(mixed0);
  mixed1 = ZeroIfNegative(mixed1);
  mixed2 = ZeroIfNegative(mixed2);

  const HWY_FULL(float) d;
  const size_t N = Lanes(d);
  mixed0 = CubeRootAndAdd(mixed0, Load(d, premul_absorb + 9 * N));
  mixed1 = CubeRootAndAdd(mixed1, Load(d, premul_absorb + 10 * N));
  mixed2 = CubeRootAndAdd(mixed2, Load(d, premul_absorb + 11 * N));
  StoreXYB(mixed0, mixed1, mixed2, valx, valy, valz);

  // For wide-gamut inputs, r/g/b and valx (but not y/z) are often negative.
}

void LinearRGBRowToXYB(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                       float* JXL_RESTRICT row2,
                       const float* JXL_RESTRICT premul_absorb, size_t xsize) {
  const HWY_FULL(float) d;
  for (size_t x = 0; x < xsize; x += Lanes(d)) {
    const auto r = Load(d, row0 + x);
    const auto g = Load(d, row1 + x);
    const auto b = Load(d, row2 + x);
    LinearRGBToXYB(r, g, b, premul_absorb, row0 + x, row1 + x, row2 + x);
  }
}

void ComputePremulAbsorb(float intensity_target, float* premul_absorb) {
  const HWY_FULL(float) d;
  const size_t N = Lanes(d);
  const float mul = intensity_target / 255.0f;
  for (size_t j = 0; j < 3; ++j) {
    for (size_t i = 0; i < 3; ++i) {
      const auto absorb = Set(d, jxl::cms::kOpsinAbsorbanceMatrix[j][i] * mul);
      Store(absorb, d, premul_absorb + (j * 3 + i) * N);
    }
  }
  for (size_t i = 0; i < 3; ++i) {
    const auto neg_bias_cbrt =
        Set(d, -cbrtf(jxl::cms::kOpsinAbsorbanceBias[i]));
    Store(neg_bias_cbrt, d, premul_absorb + (9 + i) * N);
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(LinearRGBRowToXYB);
void LinearRGBRowToXYB(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                       float* JXL_RESTRICT row2,
                       const float* JXL_RESTRICT premul_absorb, size_t xsize) {
  HWY_DYNAMIC_DISPATCH(LinearRGBRowToXYB)
  (row0, row1, row2, premul_absorb, xsize);
}

HWY_EXPORT(ComputePremulAbsorb);
void ComputePremulAbsorb(float intensity_target, float* premul_absorb) {
  HWY_DYNAMIC_DISPATCH(ComputePremulAbsorb)(intensity_target, premul_absorb);
}

void ScaleXYBRow(float* JXL_RESTRICT row0, float* JXL_RESTRICT row1,
                 float* JXL_RESTRICT row2, size_t xsize) {
  for (size_t x = 0; x < xsize; x++) {
    row2[x] = (row2[x] - row1[x] + jxl::cms::kScaledXYBOffset[2]) *
              jxl::cms::kScaledXYBScale[2];
    row0[x] = (row0[x] + jxl::cms::kScaledXYBOffset[0]) *
              jxl::cms::kScaledXYBScale[0];
    row1[x] = (row1[x] + jxl::cms::kScaledXYBOffset[1]) *
              jxl::cms::kScaledXYBScale[1];
  }
}
}  // namespace jxl
#endif  // HWY_ONCE
