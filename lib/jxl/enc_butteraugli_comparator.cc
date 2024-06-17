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
void AlphaBlend(const Image3F& in, const size_t c, float background_linear,
                const ImageF& alpha, Image3F* out) {
  const float background = LinearToSrgb8Direct(background_linear);

  for (size_t y = 0; y < out->ysize(); ++y) {
    const float* JXL_RESTRICT row_a = alpha.ConstRow(y);
    const float* JXL_RESTRICT row_i = in.ConstPlaneRow(c, y);
    float* JXL_RESTRICT row_o = out->PlaneRow(c, y);
    for (size_t x = 0; x < out->xsize(); ++x) {
      const float a = row_a[x];
      if (a <= 0.f) {
        row_o[x] = background_linear;
      } else if (a >= 1.f) {
        row_o[x] = row_i[x];
      } else {
        const float w_fg = a;
        const float w_bg = 1.0f - w_fg;
        const float fg = w_fg * LinearToSrgb8Direct(row_i[x]);
        const float bg = w_bg * background;
        row_o[x] = Srgb8ToLinearDirect(fg + bg);
      }
    }
  }
}

void AlphaBlend(float background_linear, ImageBundle* io_linear_srgb) {
  // No alpha => all opaque.
  if (!io_linear_srgb->HasAlpha()) return;

  for (size_t c = 0; c < 3; ++c) {
    AlphaBlend(*io_linear_srgb->color(), c, background_linear,
               *io_linear_srgb->alpha(), io_linear_srgb->color());
  }
}

float ComputeButteraugliImpl(const ImageBundle& ref, const ImageBundle& actual,
                             const ButteraugliParams& params,
                             const JxlCmsInterface& cms,
                             ImageF* distmap) {
  std::unique_ptr<ButteraugliComparator> comparator;
  JXL_ASSIGN_OR_DIE(comparator, ButteraugliComparator::Make(
      ref.color(), params));
  JXL_ASSIGN_OR_DIE(ImageF temp_diffmap,
                    ImageF::Create(ref.xsize(), ref.ysize()));
  JXL_CHECK(comparator->Diffmap(actual.color(), temp_diffmap));
  float score = ButteraugliScoreFromDiffmap(temp_diffmap, &params);
  if (distmap != nullptr) {
    distmap->Swap(temp_diffmap);
  }
  return score;
}

Status TransformIfNeeded(const ImageBundle& in, const ColorEncoding& c_desired,
                         const JxlCmsInterface& cms, ThreadPool* pool,
                         ImageBundle* store, const ImageBundle** out) {
  if (in.c_current().SameColorEncoding(c_desired) && !in.HasBlack()) {
    *out = &in;
    return true;
  }
  // TODO(janwas): avoid copying via createExternal+copyBackToIO
  // instead of copy+createExternal+copyBackToIO
  JXL_ASSIGN_OR_RETURN(Image3F color,
                       Image3F::Create(in.color().xsize(), in.color().ysize()));
  CopyImageTo(in.color(), &color);
  store->SetFromImage(std::move(color), in.c_current());

  // Must at least copy the alpha channel for use by external_image.
  if (in.HasExtraChannels()) {
    std::vector<ImageF> extra_channels;
    for (const ImageF& extra_channel : in.extra_channels()) {
      JXL_ASSIGN_OR_RETURN(ImageF ec, ImageF::Create(extra_channel.xsize(),
                                                     extra_channel.ysize()));
      CopyImageTo(extra_channel, &ec);
      extra_channels.emplace_back(std::move(ec));
    }
    store->SetExtraChannels(std::move(extra_channels));
  }


  if (!ApplyColorTransform(
          store->c_current(), store->metadata()->IntensityTarget(),
          *store->color(), store->HasBlack() ? &store->black() : nullptr,
          Rect(*store->color()), c_desired, cms, pool, store->color())) {
    return false;
  }
  store->OverrideProfile(c_desired);
  *out = store;
  return true;
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

  // Convert to linear sRGB (unless already in that space)
  ImageMetadata metadata0 = *rgb0.metadata();
  ImageBundle store0(&metadata0);
  const ImageBundle* linear_srgb0;
  JXL_CHECK(TransformIfNeeded(rgb0, ColorEncoding::LinearSRGB(rgb0.IsGray()),
                              cms, pool, &store0, &linear_srgb0));
  ImageMetadata metadata1 = *rgb1.metadata();
  ImageBundle store1(&metadata1);
  const ImageBundle* linear_srgb1;
  JXL_CHECK(TransformIfNeeded(rgb1, ColorEncoding::LinearSRGB(rgb1.IsGray()),
                              cms, pool, &store1, &linear_srgb1));

  // No alpha: skip blending, only need a single call to Butteraugli.
  if (ignore_alpha || (!rgb0.HasAlpha() && !rgb1.HasAlpha())) {
    return ComputeButteraugliImpl(
        *linear_srgb0, *linear_srgb1, params, cms, diffmap);
  }

  // Blend on black and white backgrounds

  const float black = 0.0f;
  JXL_ASSIGN_OR_DIE(ImageBundle blended_black0, linear_srgb0->Copy());
  JXL_ASSIGN_OR_DIE(ImageBundle blended_black1, linear_srgb1->Copy());
  AlphaBlend(black, &blended_black0);
  AlphaBlend(black, &blended_black1);

  const float white = 1.0f;
  JXL_ASSIGN_OR_DIE(ImageBundle blended_white0, linear_srgb0->Copy());
  JXL_ASSIGN_OR_DIE(ImageBundle blended_white1, linear_srgb1->Copy());

  AlphaBlend(white, &blended_white0);
  AlphaBlend(white, &blended_white1);

  ImageF diffmap_black;
  ImageF diffmap_white;
  const float dist_black = ComputeButteraugliImpl(
      blended_black0, blended_black1, params, cms, &diffmap_black);
  const float dist_white = ComputeButteraugliImpl(
      blended_white0, blended_white1, params, cms, &diffmap_white);

  // diffmap and return values are the max of diffmap_black/white.
  if (diffmap != nullptr) {
    const size_t xsize = rgb0.xsize();
    const size_t ysize = rgb0.ysize();
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
