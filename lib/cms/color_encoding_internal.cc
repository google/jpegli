// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/cms/color_encoding_internal.h"

#include <array>

#include "lib/base/common.h"
#include "lib/cms/color_encoding_cms.h"
#include "lib/cms/jxl_cms_internal.h"

namespace jxl {

std::array<ColorEncoding, 2> ColorEncoding::CreateC2(Primaries pr,
                                                     TransferFunction tf) {
  std::array<ColorEncoding, 2> c2;

  ColorEncoding* c_rgb = c2.data() + 0;
  c_rgb->SetColorSpace(ColorSpace::kRGB);
  c_rgb->storage_.white_point = WhitePoint::kD65;
  c_rgb->storage_.primaries = pr;
  c_rgb->storage_.tf.SetTransferFunction(tf);
  JXL_CHECK(c_rgb->CreateICC());

  ColorEncoding* c_gray = c2.data() + 1;
  c_gray->SetColorSpace(ColorSpace::kGray);
  c_gray->storage_.white_point = WhitePoint::kD65;
  c_gray->storage_.primaries = pr;
  c_gray->storage_.tf.SetTransferFunction(tf);
  JXL_CHECK(c_gray->CreateICC());

  return c2;
}

const ColorEncoding& ColorEncoding::SRGB(bool is_gray) {
  static std::array<ColorEncoding, 2> c2 =
      CreateC2(Primaries::kSRGB, TransferFunction::kSRGB);
  return c2[is_gray ? 1 : 0];
}
const ColorEncoding& ColorEncoding::LinearSRGB(bool is_gray) {
  static std::array<ColorEncoding, 2> c2 =
      CreateC2(Primaries::kSRGB, TransferFunction::kLinear);
  return c2[is_gray ? 1 : 0];
}

Status ColorEncoding::SetWhitePointType(const WhitePoint& wp) {
  JXL_DASSERT(storage_.have_fields);
  storage_.white_point = wp;
  return true;
}

Status ColorEncoding::SetPrimariesType(const Primaries& p) {
  JXL_DASSERT(storage_.have_fields);
  JXL_ASSERT(HasPrimaries());
  storage_.primaries = p;
  return true;
}

void ColorEncoding::DecideIfWantICC(const JxlCmsInterface& cms) {
  if (storage_.icc.empty()) return;

  JxlColorEncoding c;
  JXL_BOOL cmyk;
  if (!cms.set_fields_from_icc(cms.set_fields_data, storage_.icc.data(),
                               storage_.icc.size(), &c, &cmyk)) {
    return;
  }
  if (cmyk) return;

  std::vector<uint8_t> icc;
  if (!MaybeCreateProfile(c, &icc)) return;

  want_icc_ = false;
}

Customxy::Customxy() {
  storage_.x = 0;
  storage_.y = 0;
}

CustomTransferFunction::CustomTransferFunction() {
  storage_.have_gamma = false;
  storage_.transfer_function = TransferFunction::kSRGB;
}

}  // namespace jxl
