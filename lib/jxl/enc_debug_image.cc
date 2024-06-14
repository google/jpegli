// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/jxl/enc_debug_image.h"

#include <cstddef>
#include <cstdint>

#include "lib/jxl/base/rect.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/dec_external_image.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/image_ops.h"

namespace jxl {

namespace {
template <typename From>
StatusOr<Image3F> ConvertToFloat(const Image3<From>& from) {
  float factor = 1.0f / std::numeric_limits<From>::max();
  if (std::is_same<From, double>::value || std::is_same<From, float>::value) {
    factor = 1.0f;
  }
  JXL_ASSIGN_OR_RETURN(Image3F to, Image3F::Create(from.xsize(), from.ysize()));
  for (size_t c = 0; c < 3; ++c) {
    for (size_t y = 0; y < from.ysize(); ++y) {
      const From* const JXL_RESTRICT row_from = from.ConstPlaneRow(c, y);
      float* const JXL_RESTRICT row_to = to.PlaneRow(c, y);
      for (size_t x = 0; x < from.xsize(); ++x) {
        row_to[x] = row_from[x] * factor;
      }
    }
  }
  return to;
}

template <typename T>
Status DumpImageT(const CompressParams& cparams, const char* label,
                  const ColorEncoding& color_encoding, const Image3<T>& image) {
  return true;
}

template <typename T>
Status DumpPlaneNormalizedT(const CompressParams& cparams, const char* label,
                            const Plane<T>& image) {
  T min;
  T max;
  ImageMinMax(image, &min, &max);
  JXL_ASSIGN_OR_RETURN(Image3B normalized,
                       Image3B::Create(image.xsize(), image.ysize()));
  for (size_t c = 0; c < 3; ++c) {
    float mul = min == max ? 0 : (255.0f / (max - min));
    for (size_t y = 0; y < image.ysize(); ++y) {
      const T* JXL_RESTRICT row_in = image.ConstRow(y);
      uint8_t* JXL_RESTRICT row_out = normalized.PlaneRow(c, y);
      for (size_t x = 0; x < image.xsize(); ++x) {
        row_out[x] = static_cast<uint8_t>((row_in[x] - min) * mul);
      }
    }
  }
  return DumpImageT(cparams, label, ColorEncoding::SRGB(), normalized);
}

}  // namespace

Status DumpImage(const CompressParams& cparams, const char* label,
                 const Image3<float>& image) {
  return DumpImageT(cparams, label, ColorEncoding::SRGB(), image);
}

Status DumpImage(const CompressParams& cparams, const char* label,
                 const Image3<uint8_t>& image) {
  return DumpImageT(cparams, label, ColorEncoding::SRGB(), image);
}

Status DumpXybImage(const CompressParams& cparams, const char* label,
                    const Image3F& image) {
  return true;
}

Status DumpPlaneNormalized(const CompressParams& cparams, const char* label,
                           const Plane<float>& image) {
  return DumpPlaneNormalizedT(cparams, label, image);
}

Status DumpPlaneNormalized(const CompressParams& cparams, const char* label,
                           const Plane<uint8_t>& image) {
  return DumpPlaneNormalizedT(cparams, label, image);
}

}  // namespace jxl
