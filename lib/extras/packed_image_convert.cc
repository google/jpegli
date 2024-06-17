// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/extras/packed_image_convert.h"

#include <jxl/cms.h>
#include <jxl/color_encoding.h>
#include <jxl/types.h>

#include <cstdint>

#include "lib/jxl/base/rect.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/dec_external_image.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/enc_image_bundle.h"
#include "lib/jxl/luminance.h"

namespace jxl {
namespace extras {

Status GetColorEncoding(const PackedPixelFile& ppf,
                        ColorEncoding* color_encoding) {
  if (ppf.primary_color_representation == PackedPixelFile::kIccIsPrimary) {
    const bool is_gray = (ppf.info.num_color_channels == 1);
    IccBytes icc = ppf.icc;
    if (!color_encoding->SetICC(std::move(icc), JxlGetDefaultCms())) {
      fprintf(stderr, "Warning: error setting ICC profile, assuming SRGB\n");
      *color_encoding = ColorEncoding::SRGB(is_gray);
    } else {
      if (color_encoding->IsCMYK()) {
        // We expect gray or tri-color.
        return JXL_FAILURE("Embedded ICC is CMYK");
      }
      if (color_encoding->IsGray() != is_gray) {
        // E.g. JPG image has 3 channels, but gray ICC.
        return JXL_FAILURE("Embedded ICC does not match image color type");
      }
    }
  } else {
    JXL_RETURN_IF_ERROR(color_encoding->FromExternal(ppf.color_encoding));
    if (color_encoding->ICC().empty()) {
      return JXL_FAILURE("Failed to serialize ICC");
    }
  }
  return true;
}

float GetIntensityTarget(const extras::PackedPixelFile& ppf,
                         const ColorEncoding& c_enc) {
  if (ppf.info.intensity_target != 0) {
    return ppf.info.intensity_target;
  } else if (c_enc.Tf().IsPQ()) {
    // Peak luminance of PQ as defined by SMPTE ST 2084:2014.
    return 10000;
  } else if (c_enc.Tf().IsHLG()) {
    // Nominal display peak luminance used as a reference by
    // Rec. ITU-R BT.2100-2.
    return 1000;
  } else {
    // SDR
    return kDefaultIntensityTarget;
  }
}

Status ConvertPackedPixelFileToImage3F(const extras::PackedPixelFile& ppf,
                                       Image3F* color, ThreadPool* pool) {
  JXL_RETURN_IF_ERROR(!ppf.frames.empty());
  const extras::PackedImage& img = ppf.frames[0].color;
  size_t bits_per_sample =
      ppf.input_bitdepth.type == JXL_BIT_DEPTH_FROM_PIXEL_FORMAT
      ? extras::PackedImage::BitsPerChannel(img.format.data_type)
      : ppf.info.bits_per_sample;
  for (size_t c = 0; c < ppf.info.num_color_channels; ++c) {
    JXL_RETURN_IF_ERROR(ConvertFromExternal(
        reinterpret_cast<const uint8_t*>(img.pixels()), img.pixels_size,
        img.xsize, img.ysize, bits_per_sample, img.format, c, pool,
        &color->Plane(c)));
  }
  if (ppf.info.num_color_channels == 1) {
    CopyImageTo(color->Plane(0), &color->Plane(1));
    CopyImageTo(color->Plane(0), &color->Plane(2));
  }
  return true;
}

PackedPixelFile ConvertImage3FToPackedPixelFile(const Image3F& image,
                                                const ColorEncoding& c_enc,
                                                JxlPixelFormat format,
                                                ThreadPool* pool) {
  PackedPixelFile ppf;
  ppf.info.xsize = image.xsize();
  ppf.info.ysize = image.ysize();
  ppf.info.num_color_channels = 3;
  ppf.info.bits_per_sample = PackedImage::BitsPerChannel(format.data_type);
  ppf.info.exponent_bits_per_sample = format.data_type == JXL_TYPE_FLOAT ? 8
                                      : format.data_type == JXL_TYPE_FLOAT16
                                          ? 5
                                          : 0;
  ppf.color_encoding = c_enc.ToExternal();
  ppf.frames.clear();
  JXL_ASSIGN_OR_DIE(PackedFrame frame,
                    PackedFrame::Create(image.xsize(), image.ysize(), format));
  const ImageF* channels[3];
  for (int c = 0; c < 3; ++c) {
    channels[c] = &image.Plane(c);
  }
  bool float_samples = ppf.info.exponent_bits_per_sample > 0;
  JXL_CHECK(ConvertChannelsToExternal(
      channels, 3, ppf.info.bits_per_sample, float_samples, format.endianness,
      frame.color.stride, pool, frame.color.pixels(0, 0, 0),
      frame.color.pixels_size, PixelCallback(), Orientation::kIdentity));
  ppf.frames.emplace_back(std::move(frame));
  return ppf;
}

}  // namespace extras
}  // namespace jxl
