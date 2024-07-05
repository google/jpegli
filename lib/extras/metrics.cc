// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/extras/metrics.h"

#include <math.h>
#include <stdlib.h>

#include <atomic>
#include <limits>

#include <jxl/cms.h>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/extras/metrics.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/extras/packed_image_convert.h"
#include "lib/jxl/base/common.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/rect.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/enc_image_bundle.h"
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Add;
using hwy::HWY_NAMESPACE::GetLane;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::Rebind;

double ComputeDistanceP(const ImageF& distmap, const ButteraugliParams& params,
                        double p) {
  const double onePerPixels = 1.0 / (distmap.ysize() * distmap.xsize());
  if (std::abs(p - 3.0) < 1E-6) {
    double sum1[3] = {0.0};

// Prefer double if possible, but otherwise use float rather than scalar.
#if HWY_CAP_FLOAT64
    using T = double;
    const Rebind<float, HWY_FULL(double)> df;
#else
    using T = float;
#endif
    const HWY_FULL(T) d;
    constexpr size_t N = MaxLanes(d);
    // Manually aligned storage to avoid asan crash on clang-7 due to
    // unaligned spill.
    HWY_ALIGN T sum_totals0[N] = {0};
    HWY_ALIGN T sum_totals1[N] = {0};
    HWY_ALIGN T sum_totals2[N] = {0};

    for (size_t y = 0; y < distmap.ysize(); ++y) {
      const float* JXL_RESTRICT row = distmap.ConstRow(y);

      auto sums0 = Zero(d);
      auto sums1 = Zero(d);
      auto sums2 = Zero(d);

      size_t x = 0;
      for (; x + Lanes(d) <= distmap.xsize(); x += Lanes(d)) {
#if HWY_CAP_FLOAT64
        const auto d1 = PromoteTo(d, Load(df, row + x));
#else
        const auto d1 = Load(d, row + x);
#endif
        const auto d2 = Mul(d1, Mul(d1, d1));
        sums0 = Add(sums0, d2);
        const auto d3 = Mul(d2, d2);
        sums1 = Add(sums1, d3);
        const auto d4 = Mul(d3, d3);
        sums2 = Add(sums2, d4);
      }

      Store(Add(sums0, Load(d, sum_totals0)), d, sum_totals0);
      Store(Add(sums1, Load(d, sum_totals1)), d, sum_totals1);
      Store(Add(sums2, Load(d, sum_totals2)), d, sum_totals2);

      for (; x < distmap.xsize(); ++x) {
        const double d1 = row[x];
        double d2 = d1 * d1 * d1;
        sum1[0] += d2;
        d2 *= d2;
        sum1[1] += d2;
        d2 *= d2;
        sum1[2] += d2;
      }
    }
    double v = 0;
    v += pow(
        onePerPixels * (sum1[0] + GetLane(SumOfLanes(d, Load(d, sum_totals0)))),
        1.0 / (p * 1.0));
    v += pow(
        onePerPixels * (sum1[1] + GetLane(SumOfLanes(d, Load(d, sum_totals1)))),
        1.0 / (p * 2.0));
    v += pow(
        onePerPixels * (sum1[2] + GetLane(SumOfLanes(d, Load(d, sum_totals2)))),
        1.0 / (p * 4.0));
    v /= 3.0;
    return v;
  } else {
    static std::atomic<int> once{0};
    if (once.fetch_add(1, std::memory_order_relaxed) == 0) {
      JXL_WARNING("WARNING: using slow ComputeDistanceP");
    }
    double sum1[3] = {0.0};
    for (size_t y = 0; y < distmap.ysize(); ++y) {
      const float* JXL_RESTRICT row = distmap.ConstRow(y);
      for (size_t x = 0; x < distmap.xsize(); ++x) {
        double d2 = std::pow(row[x], p);
        sum1[0] += d2;
        d2 *= d2;
        sum1[1] += d2;
        d2 *= d2;
        sum1[2] += d2;
      }
    }
    double v = 0;
    for (int i = 0; i < 3; ++i) {
      v += pow(onePerPixels * (sum1[i]), 1.0 / (p * (1 << i)));
    }
    v /= 3.0;
    return v;
  }
}

void ComputeSumOfSquares(const extras::PackedPixelFile& a,
                         const extras::PackedPixelFile& b,
                         const JxlCmsInterface& cms, double sum_of_squares[3]) {
  const size_t xsize = a.xsize();
  const size_t ysize = a.ysize();
  const bool is_gray = a.info.num_color_channels == 1;
  // Convert to sRGB - closer to perception than linear.
  ColorEncoding c_desired = ColorEncoding::SRGB(is_gray);

  JXL_ASSIGN_OR_DIE(Image3F srgb0, Image3F::Create(xsize, ysize));
  JXL_CHECK(ConvertPackedPixelFileToImage3F(a, &srgb0, nullptr));
  JXL_ASSIGN_OR_DIE(Image3F srgb1, Image3F::Create(xsize, ysize));
  JXL_CHECK(ConvertPackedPixelFileToImage3F(b, &srgb1, nullptr));

  ColorEncoding c_enc_a;
  ColorEncoding c_enc_b;
  JXL_CHECK(GetColorEncoding(a, &c_enc_a));
  JXL_CHECK(GetColorEncoding(b, &c_enc_b));
  float intensity_a = GetIntensityTarget(a, c_enc_a);
  float intensity_b = GetIntensityTarget(b, c_enc_b);

  if (!c_enc_a.SameColorEncoding(c_desired)) {
    JXL_CHECK(ApplyColorTransform(
        c_enc_a, intensity_a, srgb0, nullptr, Rect(srgb0), c_desired, cms,
        nullptr, &srgb0));
  }
  if (!c_enc_b.SameColorEncoding(c_desired)) {
    JXL_CHECK(ApplyColorTransform(
        c_enc_b, intensity_b, srgb1, nullptr, Rect(srgb1), c_desired, cms,
        nullptr, &srgb1));
  }

  // TODO(veluca): SIMD.
  float yuvmatrix[3][3] = {{0.299, 0.587, 0.114},
                           {-0.14713, -0.28886, 0.436},
                           {0.615, -0.51499, -0.10001}};
  for (size_t y = 0; y < ysize; ++y) {
    const float* JXL_RESTRICT row0[3];
    const float* JXL_RESTRICT row1[3];
    for (size_t j = 0; j < 3; j++) {
      row0[j] = srgb0.ConstPlaneRow(j, y);
      row1[j] = srgb1.ConstPlaneRow(j, y);
    }
    for (size_t x = 0; x < xsize; ++x) {
      float cdiff[3] = {};
      // YUV conversion is linear, so we can run it on the difference.
      for (size_t j = 0; j < 3; j++) {
        cdiff[j] = row0[j][x] - row1[j][x];
      }
      float yuvdiff[3] = {};
      for (size_t j = 0; j < 3; j++) {
        for (size_t k = 0; k < 3; k++) {
          yuvdiff[j] += yuvmatrix[j][k] * cdiff[k];
        }
      }
      for (size_t j = 0; j < 3; j++) {
        sum_of_squares[j] += yuvdiff[j] * yuvdiff[j];
      }
    }
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

namespace {
float ComputeButteraugli(const Image3F& ref, const Image3F& actual,
                         const ButteraugliParams& params,
                         const JxlCmsInterface& cms,
                         ImageF* distmap) {
  std::unique_ptr<ButteraugliComparator> comparator;
  JXL_ASSIGN_OR_DIE(comparator, ButteraugliComparator::Make(ref, params));
  JXL_ASSIGN_OR_DIE(ImageF temp_distmap,
                    ImageF::Create(ref.xsize(), ref.ysize()));
  JXL_CHECK(comparator->Diffmap(actual, temp_distmap));
  float score = ButteraugliScoreFromDiffmap(temp_distmap, &params);
  if (distmap != nullptr) {
    distmap->Swap(temp_distmap);
  }
  return score;
}

}  // namespace

float ButteraugliDistance(const extras::PackedPixelFile& a,
                          const extras::PackedPixelFile& b,
                          ButteraugliParams params,
                          ImageF* distmap, ThreadPool* pool,
                          bool ignore_alpha) {
  if (a.xsize() != b.xsize() || a.ysize() != b.ysize()) {
    fprintf(stderr, "Images must have the same size for butteraugli.");
    return std::numeric_limits<float>::max();
  }
  if (a.info.num_color_channels != b.info.num_color_channels) {
    fprintf(stderr, "Grayscale vs RGB comparison not supported.");
    return std::numeric_limits<float>::max();
  }
  const size_t xsize = a.xsize();
  const size_t ysize = a.ysize();
  const bool is_gray = a.info.num_color_channels == 1;
  ColorEncoding c_desired = ColorEncoding::LinearSRGB(is_gray);
  const JxlCmsInterface& cms = *JxlGetDefaultCms();

  JXL_ASSIGN_OR_DIE(Image3F rgb0, Image3F::Create(xsize, ysize));
  JXL_CHECK(ConvertPackedPixelFileToImage3F(a, &rgb0, pool));
  JXL_ASSIGN_OR_DIE(Image3F rgb1, Image3F::Create(xsize, ysize));
  JXL_CHECK(ConvertPackedPixelFileToImage3F(b, &rgb1, pool));

  ColorEncoding c_enc_a;
  ColorEncoding c_enc_b;
  JXL_CHECK(GetColorEncoding(a, &c_enc_a));
  JXL_CHECK(GetColorEncoding(b, &c_enc_b));
  float intensity_a = GetIntensityTarget(a, c_enc_a);
  float intensity_b = GetIntensityTarget(b, c_enc_b);

  if (!c_enc_a.SameColorEncoding(c_desired)) {
    JXL_CHECK(ApplyColorTransform(
        c_enc_a, intensity_a, rgb0, nullptr, Rect(rgb0), c_desired, cms, pool,
        &rgb0));
  }
  if (!c_enc_b.SameColorEncoding(c_desired)) {
    JXL_CHECK(ApplyColorTransform(
        c_enc_b, intensity_b, rgb1, nullptr, Rect(rgb1), c_desired, cms, pool,
        &rgb1));
  }

  return ComputeButteraugli(rgb0, rgb1, params, cms, distmap);
}

HWY_EXPORT(ComputeDistanceP);
double ComputeDistanceP(const ImageF& distmap, const ButteraugliParams& params,
                        double p) {
  return HWY_DYNAMIC_DISPATCH(ComputeDistanceP)(distmap, params, p);
}

float Butteraugli3Norm(const extras::PackedPixelFile& a,
                       const extras::PackedPixelFile& b, ThreadPool* pool) {
  ButteraugliParams params;
  ImageF distmap;
  ButteraugliDistance(a, b, params, &distmap, pool);
  return ComputeDistanceP(distmap, params, 3);
}

HWY_EXPORT(ComputeSumOfSquares);
double ComputePSNR(const extras::PackedPixelFile& a,
                   const extras::PackedPixelFile& b,
                   const JxlCmsInterface& cms) {
  if (a.xsize() != b.xsize() || a.ysize() != b.ysize()) {
    fprintf(stderr, "Images must have the same size for PSNR.");
    return 0.0;
  }
  if (a.info.num_color_channels != b.info.num_color_channels) {
    fprintf(stderr, "Grayscale vs RGB comparison not supported.");
    return 0.0;
  }
  double sum_of_squares[3] = {};
  HWY_DYNAMIC_DISPATCH(ComputeSumOfSquares)(a, b, cms, sum_of_squares);
  constexpr double kChannelWeights[3] = {6.0 / 8, 1.0 / 8, 1.0 / 8};
  double avg_psnr = 0;
  const size_t input_pixels = a.xsize() * a.ysize();
  for (int i = 0; i < 3; ++i) {
    const double rmse = std::sqrt(sum_of_squares[i] / input_pixels);
    const double psnr =
        sum_of_squares[i] == 0 ? 99.99 : (20 * std::log10(1 / rmse));
    avg_psnr += kChannelWeights[i] * psnr;
  }
  return avg_psnr;
}

}  // namespace jxl
#endif
