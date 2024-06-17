// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/jxl/enc_butteraugli_comparator.h"

#include "lib/jxl/base/status.h"
#include "lib/jxl/butteraugli/butteraugli.h"
#include "lib/jxl/enc_gamma_correct.h"
#include "lib/jxl/enc_image_bundle.h"

#include <memory>

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
  JXL_ASSIGN_OR_DIE(ImageF temp_diffmap,
                    ImageF::Create(ref.xsize(), ref.ysize()));
  JXL_CHECK(comparator->Diffmap(actual, temp_diffmap));
  float score = ButteraugliScoreFromDiffmap(temp_diffmap, &params);
  if (distmap != nullptr) {
    distmap->Swap(temp_diffmap);
  }
  return score;
}

}  // namespace

float ButteraugliDistance(const ImageBundle& rgb0, const ImageBundle& rgb1,
                          const ButteraugliParams& params,
                          const JxlCmsInterface& cms, ImageF* diffmap,
                          ThreadPool* pool, bool ignore_alpha) {
  if (rgb0.xsize() != rgb1.xsize() || rgb0.ysize() != rgb1.ysize()) {
    fprintf(stderr, "Images must have the same size for butteraugli.");
    return std::numeric_limits<float>::max();
  }
  if (rgb0.IsGray() != rgb1.IsGray()) {
    fprintf(stderr, "Grayscale vs RGB comparison not supported.");
    return std::numeric_limits<float>::max();
  }
  const size_t xsize = rgb0.xsize();
  const size_t ysize = rgb0.ysize();
  const bool is_gray = rgb0.IsGray();
  ColorEncoding c_desired = ColorEncoding::LinearSRGB(is_gray);

  JXL_ASSIGN_OR_DIE(Image3F linear_srgb0, Image3F::Create(xsize, ysize));
  if (rgb0.c_current().SameColorEncoding(c_desired) && !rgb0.HasBlack()) {
    CopyImageTo(rgb0.color(), &linear_srgb0);
  } else{
    JXL_CHECK(ApplyColorTransform(
        rgb0.c_current(), rgb0.metadata()->IntensityTarget(),
        rgb0.color(), rgb0.HasBlack() ? &rgb0.black() : nullptr,
        Rect(rgb0.color()), c_desired, cms, pool, &linear_srgb0));
  }
  JXL_ASSIGN_OR_DIE(Image3F linear_srgb1, Image3F::Create(xsize, ysize));
  if (rgb1.c_current().SameColorEncoding(c_desired) && !rgb1.HasBlack()) {
    CopyImageTo(rgb1.color(), &linear_srgb1);
  } else{
    JXL_CHECK(ApplyColorTransform(
        rgb1.c_current(), rgb1.metadata()->IntensityTarget(),
        rgb1.color(), rgb1.HasBlack() ? &rgb1.black() : nullptr,
        Rect(rgb1.color()), c_desired, cms, pool, &linear_srgb1));
  }

  // No alpha: skip blending, only need a single call to Butteraugli.
  if (ignore_alpha || (!rgb0.HasAlpha() && !rgb1.HasAlpha())) {
    return ComputeButteraugli(linear_srgb0, linear_srgb1, params, cms, diffmap);
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

  ImageF diffmap_black;
  ImageF diffmap_white;
  const float dist_black = ComputeButteraugli(
      blended_black0, blended_black1, params, cms, &diffmap_black);
  const float dist_white = ComputeButteraugli(
      blended_white0, blended_white1, params, cms, &diffmap_white);

  // diffmap and return values are the max of diffmap_black/white.
  if (diffmap != nullptr) {
    JXL_ASSIGN_OR_DIE(*diffmap, ImageF::Create(xsize, ysize));
    for (size_t y = 0; y < ysize; ++y) {
      const float* JXL_RESTRICT row_black = diffmap_black.ConstRow(y);
      const float* JXL_RESTRICT row_white = diffmap_white.ConstRow(y);
      float* JXL_RESTRICT row_out = diffmap->Row(y);
      for (size_t x = 0; x < xsize; ++x) {
        row_out[x] = std::max(row_black[x], row_white[x]);
      }
    }
  }
  return std::max(dist_black, dist_white);
}

}  // namespace jxl
