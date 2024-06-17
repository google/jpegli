// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/jxl/enc_butteraugli_comparator.h"

#include "lib/jxl/base/status.h"
#include "lib/jxl/enc_gamma_correct.h"
#include "lib/jxl/enc_image_bundle.h"

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

class JxlButteraugliComparator {
 public:
  explicit JxlButteraugliComparator(const ButteraugliParams& params,
                                    const JxlCmsInterface& cms);

  Status SetReferenceImage(const ImageBundle& ref);
  Status SetLinearReferenceImage(const Image3F& linear);

  Status CompareWith(const ImageBundle& actual, ImageF* diffmap,
                     float* score);

  float GoodQualityScore() const;
  float BadQualityScore() const;

 private:
  ButteraugliParams params_;
  JxlCmsInterface cms_;
  std::unique_ptr<ButteraugliComparator> comparator_;
  size_t xsize_ = 0;
  size_t ysize_ = 0;
};

float ComputeScoreImpl(const ImageBundle& rgb0, const ImageBundle& rgb1,
                       const ButteraugliParams& params,
                       const JxlCmsInterface& cms,
                       ImageF* distmap) {
  JxlButteraugliComparator comparator(params, cms);
  JXL_CHECK(comparator.SetReferenceImage(rgb0));
  float score;
  JXL_CHECK(comparator.CompareWith(rgb1, distmap, &score));
  return score;
}
}  // namespace

JxlButteraugliComparator::JxlButteraugliComparator(
    const ButteraugliParams& params, const JxlCmsInterface& cms)
    : params_(params), cms_(cms) {}

Status JxlButteraugliComparator::SetReferenceImage(const ImageBundle& ref) {
  const ImageBundle* ref_linear_srgb;
  ImageMetadata metadata = *ref.metadata();
  ImageBundle store(&metadata);
  if (!TransformIfNeeded(ref, ColorEncoding::LinearSRGB(ref.IsGray()), cms_,
                         /*pool=*/nullptr, &store, &ref_linear_srgb)) {
    return false;
  }
  JXL_ASSIGN_OR_RETURN(comparator_, ButteraugliComparator::Make(
                                        ref_linear_srgb->color(), params_));
  xsize_ = ref.xsize();
  ysize_ = ref.ysize();
  return true;
}

Status JxlButteraugliComparator::SetLinearReferenceImage(
    const Image3F& linear) {
  JXL_ASSIGN_OR_RETURN(comparator_,
                       ButteraugliComparator::Make(linear, params_));
  xsize_ = linear.xsize();
  ysize_ = linear.ysize();
  return true;
}

Status JxlButteraugliComparator::CompareWith(const ImageBundle& actual,
                                             ImageF* diffmap, float* score) {
  if (!comparator_) {
    return JXL_FAILURE("Must set reference image first");
  }
  if (xsize_ != actual.xsize() || ysize_ != actual.ysize()) {
    return JXL_FAILURE("Images must have same size");
  }

  const ImageBundle* actual_linear_srgb;
  ImageMetadata metadata = *actual.metadata();
  ImageBundle store(&metadata);
  if (!TransformIfNeeded(actual, ColorEncoding::LinearSRGB(actual.IsGray()),
                         cms_,
                         /*pool=*/nullptr, &store, &actual_linear_srgb)) {
    return false;
  }

  JXL_ASSIGN_OR_RETURN(ImageF temp_diffmap, ImageF::Create(xsize_, ysize_));
  JXL_RETURN_IF_ERROR(
      comparator_->Diffmap(actual_linear_srgb->color(), temp_diffmap));

  if (score != nullptr) {
    *score = ButteraugliScoreFromDiffmap(temp_diffmap, &params_);
  }
  if (diffmap != nullptr) {
    diffmap->Swap(temp_diffmap);
  }

  return true;
}

float JxlButteraugliComparator::GoodQualityScore() const {
  return ButteraugliFuzzyInverse(1.5);
}

float JxlButteraugliComparator::BadQualityScore() const {
  return ButteraugliFuzzyInverse(0.5);
}

Status ComputeScore(const ImageBundle& rgb0, const ImageBundle& rgb1,
                    const ButteraugliParams& params,
                    const JxlCmsInterface& cms,
                    float* score, ImageF* diffmap, ThreadPool* pool,
                    bool ignore_alpha) {
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
    *score =
        ComputeScoreImpl(*linear_srgb0, *linear_srgb1, params, cms, diffmap);
    return true;
  }

  // Blend on black and white backgrounds

  const float black = 0.0f;
  JXL_ASSIGN_OR_RETURN(ImageBundle blended_black0, linear_srgb0->Copy());
  JXL_ASSIGN_OR_RETURN(ImageBundle blended_black1, linear_srgb1->Copy());
  AlphaBlend(black, &blended_black0);
  AlphaBlend(black, &blended_black1);

  const float white = 1.0f;
  JXL_ASSIGN_OR_RETURN(ImageBundle blended_white0, linear_srgb0->Copy());
  JXL_ASSIGN_OR_RETURN(ImageBundle blended_white1, linear_srgb1->Copy());

  AlphaBlend(white, &blended_white0);
  AlphaBlend(white, &blended_white1);

  ImageF diffmap_black;
  ImageF diffmap_white;
  const float dist_black = ComputeScoreImpl(blended_black0, blended_black1,
                                            params, cms, &diffmap_black);
  const float dist_white = ComputeScoreImpl(blended_white0, blended_white1,
                                            params, cms, &diffmap_white);

  // diffmap and return values are the max of diffmap_black/white.
  if (diffmap != nullptr) {
    const size_t xsize = rgb0.xsize();
    const size_t ysize = rgb0.ysize();
    JXL_ASSIGN_OR_RETURN(*diffmap, ImageF::Create(xsize, ysize));
    for (size_t y = 0; y < ysize; ++y) {
      const float* JXL_RESTRICT row_black = diffmap_black.ConstRow(y);
      const float* JXL_RESTRICT row_white = diffmap_white.ConstRow(y);
      float* JXL_RESTRICT row_out = diffmap->Row(y);
      for (size_t x = 0; x < xsize; ++x) {
        row_out[x] = std::max(row_black[x], row_white[x]);
      }
    }
  }
  *score = std::max(dist_black, dist_white);
  return true;
}

}  // namespace jxl
