// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/extras/metrics.h"

#include <math.h>
#include <stdlib.h>

#include <atomic>

#include <jxl/cms.h>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/extras/metrics.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/extras/packed_image_convert.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/rect.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/enc_gamma_correct.h"
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

void ComputeSumOfSquares(const ImageBundle& ib1, const ImageBundle& ib2,
                         const JxlCmsInterface& cms, double sum_of_squares[3]) {
  // Convert to sRGB - closer to perception than linear.
  const Image3F* srgb1 = &ib1.color();
  Image3F copy1;
  if (!ib1.IsSRGB()) {
    JXL_CHECK(ApplyColorTransform(
        ib1.c_current(), ib1.metadata()->IntensityTarget(), ib1.color(),
        ib1.HasBlack() ? &ib1.black() : nullptr, Rect(ib1),
        ColorEncoding::SRGB(ib1.IsGray()), cms, nullptr, &copy1));
    srgb1 = &copy1;
  }
  const Image3F* srgb2 = &ib2.color();
  Image3F copy2;
  if (!ib2.IsSRGB()) {
    JXL_CHECK(ApplyColorTransform(
        ib2.c_current(), ib2.metadata()->IntensityTarget(), ib2.color(),
        ib2.HasBlack() ? &ib2.black() : nullptr, Rect(ib2),
        ColorEncoding::SRGB(ib2.IsGray()), cms, nullptr, &copy2));
    srgb2 = &copy2;
  }

  JXL_CHECK(SameSize(*srgb1, *srgb2));

  // TODO(veluca): SIMD.
  float yuvmatrix[3][3] = {{0.299, 0.587, 0.114},
                           {-0.14713, -0.28886, 0.436},
                           {0.615, -0.51499, -0.10001}};
  for (size_t y = 0; y < srgb1->ysize(); ++y) {
    const float* JXL_RESTRICT row1[3];
    const float* JXL_RESTRICT row2[3];
    for (size_t j = 0; j < 3; j++) {
      row1[j] = srgb1->ConstPlaneRow(j, y);
      row2[j] = srgb2->ConstPlaneRow(j, y);
    }
    for (size_t x = 0; x < srgb1->xsize(); ++x) {
      float cdiff[3] = {};
      // YUV conversion is linear, so we can run it on the difference.
      for (size_t j = 0; j < 3; j++) {
        cdiff[j] = row1[j][x] - row2[j][x];
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
// color is linear, but blending happens in gamma-compressed space using
// (gamma-compressed) grayscale background color, alpha image represents
// weights of the sRGB colors in the [0 .. (1 << bit_depth) - 1] interval,
// output image is in linear space.
void AlphaBlend(float background_linear, const ImageF& alpha, Image3F* inout) {
  const float background = LinearToSrgb8Direct(background_linear);

  for (size_t y = 0; y < inout->ysize(); ++y) {
    const float* JXL_RESTRICT row_a = alpha.ConstRow(y);
    for (size_t c = 0; c < 3; ++c) {
      float* JXL_RESTRICT row = inout->PlaneRow(c, y);
      for (size_t x = 0; x < inout->xsize(); ++x) {
        const float a = row_a[x];
        if (a <= 0.f) {
          row[x] = background_linear;
        } else if (a < 1.f) {
          const float w_fg = a;
          const float w_bg = 1.0f - w_fg;
          const float fg = w_fg * LinearToSrgb8Direct(row[x]);
          const float bg = w_bg * background;
          row[x] = Srgb8ToLinearDirect(fg + bg);
        }
      }
    }
  }
}

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
  const size_t ysize = b.ysize();
  const bool is_gray = a.info.num_color_channels == 1;
  ColorEncoding c_desired = ColorEncoding::LinearSRGB(is_gray);

  CodecInOut io0;
  JXL_CHECK(ConvertPackedPixelFileToCodecInOut(a, pool, &io0));
  CodecInOut io1;
  JXL_CHECK(ConvertPackedPixelFileToCodecInOut(b, pool, &io1));
  const ImageBundle& rgb0 = io0.frames[0];
  const ImageBundle& rgb1 = io1.frames[0];
  const JxlCmsInterface& cms = *JxlGetDefaultCms();

  ColorEncoding c_enc_a;
  ColorEncoding c_enc_b;
  JXL_CHECK(GetColorEncoding(a, &c_enc_a));
  JXL_CHECK(GetColorEncoding(b, &c_enc_b));

  JXL_ASSIGN_OR_DIE(Image3F linear_srgb0, Image3F::Create(xsize, ysize));
  if (c_enc_a.SameColorEncoding(c_desired) && !rgb0.HasBlack()) {
    CopyImageTo(rgb0.color(), &linear_srgb0);
  } else{
    JXL_CHECK(ApplyColorTransform(
        c_enc_a, rgb0.metadata()->IntensityTarget(),
        rgb0.color(), rgb0.HasBlack() ? &rgb0.black() : nullptr,
        Rect(rgb0.color()), c_desired, cms, pool, &linear_srgb0));
  }
  JXL_ASSIGN_OR_DIE(Image3F linear_srgb1, Image3F::Create(xsize, ysize));
  if (c_enc_b.SameColorEncoding(c_desired) && !rgb1.HasBlack()) {
    CopyImageTo(rgb1.color(), &linear_srgb1);
  } else{
    JXL_CHECK(ApplyColorTransform(
        c_enc_b, rgb1.metadata()->IntensityTarget(),
        rgb1.color(), rgb1.HasBlack() ? &rgb1.black() : nullptr,
        Rect(rgb1.color()), c_desired, cms, pool, &linear_srgb1));
  }

  // No alpha: skip blending, only need a single call to Butteraugli.
  if (ignore_alpha || (!rgb0.HasAlpha() && !rgb1.HasAlpha())) {
    return ComputeButteraugli(linear_srgb0, linear_srgb1, params, cms, distmap);
  }

  // Blend on black and white backgrounds

  const float black = 0.0f;
  const float white = 1.0f;
  JXL_ASSIGN_OR_DIE(Image3F blended_black0, Image3F::Create(xsize, ysize));
  JXL_ASSIGN_OR_DIE(Image3F blended_white0, Image3F::Create(xsize, ysize));
  CopyImageTo(linear_srgb0, &blended_black0);
  CopyImageTo(linear_srgb0, &blended_white0);
  if (rgb0.HasAlpha()) {
    AlphaBlend(black, rgb0.alpha(), &blended_black0);
    AlphaBlend(white, rgb0.alpha(), &blended_white0);
  }

  JXL_ASSIGN_OR_DIE(Image3F blended_black1, Image3F::Create(xsize, ysize));
  JXL_ASSIGN_OR_DIE(Image3F blended_white1, Image3F::Create(xsize, ysize));
  CopyImageTo(linear_srgb1, &blended_black1);
  CopyImageTo(linear_srgb1, &blended_white1);
  if (rgb1.HasAlpha()) {
    AlphaBlend(black, rgb1.alpha(), &blended_black1);
    AlphaBlend(white, rgb1.alpha(), &blended_white1);
  }

  ImageF distmap_black;
  ImageF distmap_white;
  const float dist_black = ComputeButteraugli(
      blended_black0, blended_black1, params, cms, &distmap_black);
  const float dist_white = ComputeButteraugli(
      blended_white0, blended_white1, params, cms, &distmap_white);

  // distmap and return values are the max of distmap_black/white.
  if (distmap != nullptr) {
    JXL_ASSIGN_OR_DIE(*distmap, ImageF::Create(xsize, ysize));
    for (size_t y = 0; y < ysize; ++y) {
      const float* JXL_RESTRICT row_black = distmap_black.ConstRow(y);
      const float* JXL_RESTRICT row_white = distmap_white.ConstRow(y);
      float* JXL_RESTRICT row_out = distmap->Row(y);
      for (size_t x = 0; x < xsize; ++x) {
        row_out[x] = std::max(row_black[x], row_white[x]);
      }
    }
  }
  return std::max(dist_black, dist_white);
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
double ComputePSNR(const ImageBundle& ib1, const ImageBundle& ib2,
                   const JxlCmsInterface& cms) {
  if (!SameSize(ib1, ib2)) return 0.0;
  double sum_of_squares[3] = {};
  HWY_DYNAMIC_DISPATCH(ComputeSumOfSquares)(ib1, ib2, cms, sum_of_squares);
  constexpr double kChannelWeights[3] = {6.0 / 8, 1.0 / 8, 1.0 / 8};
  double avg_psnr = 0;
  const size_t input_pixels = ib1.xsize() * ib1.ysize();
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
