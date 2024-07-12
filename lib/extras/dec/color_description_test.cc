// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/extras/dec/color_description.h"

#include "lib/base/testing.h"
#include "lib/cms/color_encoding_internal.h"

namespace jxl {

// A POD descriptor of a ColorEncoding. Only used in tests as the return value
// of AllEncodings().
struct ColorEncodingDescriptor {
  ColorSpace color_space;
  WhitePoint white_point;
  Primaries primaries;
  TransferFunction tf;
  RenderingIntent rendering_intent;
};

ColorEncoding ColorEncodingFromDescriptor(const ColorEncodingDescriptor& desc) {
  ColorEncoding c;
  c.SetColorSpace(desc.color_space);
  if (desc.color_space != ColorSpace::kXYB) {
    JXL_CHECK(c.SetWhitePointType(desc.white_point));
    if (desc.color_space != ColorSpace::kGray) {
      JXL_CHECK(c.SetPrimariesType(desc.primaries));
    }
    c.Tf().SetTransferFunction(desc.tf);
  }
  c.SetRenderingIntent(desc.rendering_intent);
  JXL_CHECK(c.CreateICC());
  return c;
}

// Define the operator<< for tests.
static inline ::std::ostream& operator<<(::std::ostream& os,
                                         const ColorEncodingDescriptor& c) {
  return os << "ColorEncoding/" << Description(ColorEncodingFromDescriptor(c));
}

// Returns ColorEncodingDescriptors, which are only used in tests. To obtain a
// ColorEncoding object call ColorEncodingFromDescriptor and then call
// ColorEncoding::CreateProfile() on that object to generate a profile.
std::vector<ColorEncodingDescriptor> AllEncodings() {
  std::vector<ColorEncodingDescriptor> all_encodings;
  all_encodings.reserve(300);

  for (ColorSpace cs : Values<ColorSpace>()) {
    if (cs == ColorSpace::kUnknown || cs == ColorSpace::kXYB ||
        cs == ColorSpace::kGray) {
      continue;
    }

    for (WhitePoint wp : Values<WhitePoint>()) {
      if (wp == WhitePoint::kCustom) continue;
      for (Primaries primaries : Values<Primaries>()) {
        if (primaries == Primaries::kCustom) continue;
        for (TransferFunction tf : Values<TransferFunction>()) {
          if (tf == TransferFunction::kUnknown) continue;
          for (RenderingIntent ri : Values<RenderingIntent>()) {
            ColorEncodingDescriptor cdesc;
            cdesc.color_space = cs;
            cdesc.white_point = wp;
            cdesc.primaries = primaries;
            cdesc.tf = tf;
            cdesc.rendering_intent = ri;
            all_encodings.push_back(cdesc);
          }
        }
      }
    }
  }

  return all_encodings;
}

// Verify ParseDescription(Description) yields the same ColorEncoding
TEST(ColorDescriptionTest, RoundTripAll) {
  for (const auto& cdesc : AllEncodings()) {
    const ColorEncoding c_original = ColorEncodingFromDescriptor(cdesc);
    const std::string description = Description(c_original);
    printf("%s\n", description.c_str());

    JxlColorEncoding c_external = {};
    EXPECT_TRUE(ParseDescription(description, &c_external));
    ColorEncoding c_internal;
    EXPECT_TRUE(c_internal.FromExternal(c_external));
    EXPECT_TRUE(c_original.SameColorEncoding(c_internal))
        << "Where c_original=" << c_original
        << " and c_internal=" << c_internal;
  }
}

TEST(ColorDescriptionTest, NanGamma) {
  const std::string description = "Gra_2_Per_gnan";
  JxlColorEncoding c;
  EXPECT_FALSE(ParseDescription(description, &c));
}

}  // namespace jxl
