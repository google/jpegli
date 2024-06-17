// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/jxl/test_utils.h"

#include <jxl/cms.h>
#include <jxl/cms_interface.h>
#include <jxl/types.h>

#include <cstddef>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lib/extras/metrics.h"
#include "lib/extras/packed_image_convert.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/float.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/padded_bytes.h"

#if !defined(TEST_DATA_PATH)
#include "tools/cpp/runfiles/runfiles.h"
#endif

namespace jxl {
namespace test {

#if defined(TEST_DATA_PATH)
std::string GetTestDataPath(const std::string& filename) {
  return std::string(TEST_DATA_PATH "/") + filename;
}
#else
using bazel::tools::cpp::runfiles::Runfiles;
const std::unique_ptr<Runfiles> kRunfiles(Runfiles::Create(""));
std::string GetTestDataPath(const std::string& filename) {
  std::string root(JPEGXL_ROOT_PACKAGE "/testdata/");
  return kRunfiles->Rlocation(root + filename);
}
#endif

std::vector<uint8_t> ReadTestData(const std::string& filename) {
  std::string full_path = GetTestDataPath(filename);
  fprintf(stderr, "ReadTestData %s\n", full_path.c_str());
  std::ifstream file(full_path, std::ios::binary);
  std::vector<char> str((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
  JXL_CHECK(file.good());
  const uint8_t* raw = reinterpret_cast<const uint8_t*>(str.data());
  std::vector<uint8_t> data(raw, raw + str.size());
  printf("Test data %s is %d bytes long.\n", filename.c_str(),
         static_cast<int>(data.size()));
  return data;
}

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

jxl::CodecInOut SomeTestImageToCodecInOut(const std::vector<uint8_t>& buf,
                                          size_t num_channels, size_t xsize,
                                          size_t ysize) {
  jxl::CodecInOut io;
  io.SetSize(xsize, ysize);
  io.metadata.m.SetAlphaBits(16);
  io.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(
      /*is_gray=*/num_channels == 1 || num_channels == 2);
  JxlPixelFormat format = {static_cast<uint32_t>(num_channels), JXL_TYPE_UINT16,
                           JXL_BIG_ENDIAN, 0};
  JXL_CHECK(ConvertFromExternal(
      jxl::Bytes(buf.data(), buf.size()), xsize, ysize,
      jxl::ColorEncoding::SRGB(/*is_gray=*/num_channels < 3),
      /*bits_per_sample=*/16, format,
      /*pool=*/nullptr,
      /*ib=*/&io.Main()));
  return io;
}

bool Near(double expected, double value, double max_dist) {
  double dist = expected > value ? expected - value : value - expected;
  return dist <= max_dist;
}

float LoadLEFloat16(const uint8_t* p) {
  uint16_t bits16 = LoadLE16(p);
  return detail::LoadFloat16(bits16);
}

float LoadBEFloat16(const uint8_t* p) {
  uint16_t bits16 = LoadBE16(p);
  return detail::LoadFloat16(bits16);
}

size_t GetPrecision(JxlDataType data_type) {
  switch (data_type) {
    case JXL_TYPE_UINT8:
      return 8;
    case JXL_TYPE_UINT16:
      return 16;
    case JXL_TYPE_FLOAT:
      // Floating point mantissa precision
      return 24;
    case JXL_TYPE_FLOAT16:
      return 11;
    default:
      JXL_ABORT("Unhandled JxlDataType");
  }
}

size_t GetDataBits(JxlDataType data_type) {
  switch (data_type) {
    case JXL_TYPE_UINT8:
      return 8;
    case JXL_TYPE_UINT16:
      return 16;
    case JXL_TYPE_FLOAT:
      return 32;
    case JXL_TYPE_FLOAT16:
      return 16;
    default:
      JXL_ABORT("Unhandled JxlDataType");
  }
}

std::vector<double> ConvertToRGBA32(const uint8_t* pixels, size_t xsize,
                                    size_t ysize, const JxlPixelFormat& format,
                                    double factor) {
  std::vector<double> result(xsize * ysize * 4);
  size_t num_channels = format.num_channels;
  bool gray = num_channels == 1 || num_channels == 2;
  bool alpha = num_channels == 2 || num_channels == 4;
  JxlEndianness endianness = format.endianness;
  // Compute actual type:
  if (endianness == JXL_NATIVE_ENDIAN) {
    endianness = IsLittleEndian() ? JXL_LITTLE_ENDIAN : JXL_BIG_ENDIAN;
  }

  size_t stride =
      xsize * jxl::DivCeil(GetDataBits(format.data_type) * num_channels,
                           jxl::kBitsPerByte);
  if (format.align > 1) stride = jxl::RoundUpTo(stride, format.align);

  if (format.data_type == JXL_TYPE_UINT8) {
    // Multiplier to bring to 0-1.0 range
    double mul = factor > 0.0 ? factor : 1.0 / 255.0;
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels;
        double r = pixels[i];
        double g = gray ? r : pixels[i + 1];
        double b = gray ? r : pixels[i + 2];
        double a = alpha ? pixels[i + num_channels - 1] : 255;
        result[j + 0] = r * mul;
        result[j + 1] = g * mul;
        result[j + 2] = b * mul;
        result[j + 3] = a * mul;
      }
    }
  } else if (format.data_type == JXL_TYPE_UINT16) {
    JXL_ASSERT(endianness != JXL_NATIVE_ENDIAN);
    // Multiplier to bring to 0-1.0 range
    double mul = factor > 0.0 ? factor : 1.0 / 65535.0;
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels * 2;
        double r;
        double g;
        double b;
        double a;
        if (endianness == JXL_BIG_ENDIAN) {
          r = (pixels[i + 0] << 8) + pixels[i + 1];
          g = gray ? r : (pixels[i + 2] << 8) + pixels[i + 3];
          b = gray ? r : (pixels[i + 4] << 8) + pixels[i + 5];
          a = alpha ? (pixels[i + num_channels * 2 - 2] << 8) +
                          pixels[i + num_channels * 2 - 1]
                    : 65535;
        } else {
          r = (pixels[i + 1] << 8) + pixels[i + 0];
          g = gray ? r : (pixels[i + 3] << 8) + pixels[i + 2];
          b = gray ? r : (pixels[i + 5] << 8) + pixels[i + 4];
          a = alpha ? (pixels[i + num_channels * 2 - 1] << 8) +
                          pixels[i + num_channels * 2 - 2]
                    : 65535;
        }
        result[j + 0] = r * mul;
        result[j + 1] = g * mul;
        result[j + 2] = b * mul;
        result[j + 3] = a * mul;
      }
    }
  } else if (format.data_type == JXL_TYPE_FLOAT) {
    JXL_ASSERT(endianness != JXL_NATIVE_ENDIAN);
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels * 4;
        double r;
        double g;
        double b;
        double a;
        if (endianness == JXL_BIG_ENDIAN) {
          r = LoadBEFloat(pixels + i);
          g = gray ? r : LoadBEFloat(pixels + i + 4);
          b = gray ? r : LoadBEFloat(pixels + i + 8);
          a = alpha ? LoadBEFloat(pixels + i + num_channels * 4 - 4) : 1.0;
        } else {
          r = LoadLEFloat(pixels + i);
          g = gray ? r : LoadLEFloat(pixels + i + 4);
          b = gray ? r : LoadLEFloat(pixels + i + 8);
          a = alpha ? LoadLEFloat(pixels + i + num_channels * 4 - 4) : 1.0;
        }
        result[j + 0] = r;
        result[j + 1] = g;
        result[j + 2] = b;
        result[j + 3] = a;
      }
    }
  } else if (format.data_type == JXL_TYPE_FLOAT16) {
    JXL_ASSERT(endianness != JXL_NATIVE_ENDIAN);
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels * 2;
        double r;
        double g;
        double b;
        double a;
        if (endianness == JXL_BIG_ENDIAN) {
          r = LoadBEFloat16(pixels + i);
          g = gray ? r : LoadBEFloat16(pixels + i + 2);
          b = gray ? r : LoadBEFloat16(pixels + i + 4);
          a = alpha ? LoadBEFloat16(pixels + i + num_channels * 2 - 2) : 1.0;
        } else {
          r = LoadLEFloat16(pixels + i);
          g = gray ? r : LoadLEFloat16(pixels + i + 2);
          b = gray ? r : LoadLEFloat16(pixels + i + 4);
          a = alpha ? LoadLEFloat16(pixels + i + num_channels * 2 - 2) : 1.0;
        }
        result[j + 0] = r;
        result[j + 1] = g;
        result[j + 2] = b;
        result[j + 3] = a;
      }
    }
  } else {
    JXL_ASSERT(false);  // Unsupported type
  }
  return result;
}

size_t ComparePixels(const uint8_t* a, const uint8_t* b, size_t xsize,
                     size_t ysize, const JxlPixelFormat& format_a,
                     const JxlPixelFormat& format_b,
                     double threshold_multiplier) {
  // Convert both images to equal full precision for comparison.
  std::vector<double> a_full = ConvertToRGBA32(a, xsize, ysize, format_a);
  std::vector<double> b_full = ConvertToRGBA32(b, xsize, ysize, format_b);
  bool gray_a = format_a.num_channels < 3;
  bool gray_b = format_b.num_channels < 3;
  bool alpha_a = ((format_a.num_channels & 1) == 0);
  bool alpha_b = ((format_b.num_channels & 1) == 0);
  size_t bits_a = GetPrecision(format_a.data_type);
  size_t bits_b = GetPrecision(format_b.data_type);
  size_t bits = std::min(bits_a, bits_b);
  // How much distance is allowed in case of pixels with lower bit depths, given
  // that the double precision float images use range 0-1.0.
  // E.g. in case of 1-bit this is 0.5 since 0.499 must map to 0 and 0.501 must
  // map to 1.
  double precision = 0.5 * threshold_multiplier / ((1ull << bits) - 1ull);
  if (format_a.data_type == JXL_TYPE_FLOAT16 ||
      format_b.data_type == JXL_TYPE_FLOAT16) {
    // Lower the precision for float16, because it currently looks like the
    // scalar and wasm implementations of hwy have 1 less bit of precision
    // than the x86 implementations.
    // TODO(lode): Set the required precision back to 11 bits when possible.
    precision = 0.5 * threshold_multiplier / ((1ull << (bits - 1)) - 1ull);
  }
  if (format_b.data_type == JXL_TYPE_UINT8) {
    // Increase the threshold by the maximum difference introduced by dithering.
    precision += 63.0 / 128.0;
  }
  size_t numdiff = 0;
  for (size_t y = 0; y < ysize; y++) {
    for (size_t x = 0; x < xsize; x++) {
      size_t i = (y * xsize + x) * 4;
      bool ok = true;
      if (gray_a || gray_b) {
        if (!Near(a_full[i + 0], b_full[i + 0], precision)) ok = false;
        // If the input was grayscale and the output not, then the output must
        // have all channels equal.
        if (gray_a && b_full[i + 0] != b_full[i + 1] &&
            b_full[i + 2] != b_full[i + 2]) {
          ok = false;
        }
      } else {
        if (!Near(a_full[i + 0], b_full[i + 0], precision) ||
            !Near(a_full[i + 1], b_full[i + 1], precision) ||
            !Near(a_full[i + 2], b_full[i + 2], precision)) {
          ok = false;
        }
      }
      if (alpha_a && alpha_b) {
        if (!Near(a_full[i + 3], b_full[i + 3], precision)) ok = false;
      } else {
        // If the input had no alpha channel, the output should be opaque
        // after roundtrip.
        if (alpha_b && !Near(1.0, b_full[i + 3], precision)) ok = false;
      }
      if (!ok) numdiff++;
    }
  }
  return numdiff;
}

double DistanceRMS(const uint8_t* a, const uint8_t* b, size_t xsize,
                   size_t ysize, const JxlPixelFormat& format) {
  // Convert both images to equal full precision for comparison.
  std::vector<double> a_full = ConvertToRGBA32(a, xsize, ysize, format);
  std::vector<double> b_full = ConvertToRGBA32(b, xsize, ysize, format);
  double sum = 0.0;
  for (size_t y = 0; y < ysize; y++) {
    double row_sum = 0.0;
    for (size_t x = 0; x < xsize; x++) {
      size_t i = (y * xsize + x) * 4;
      for (size_t c = 0; c < format.num_channels; ++c) {
        double diff = a_full[i + c] - b_full[i + c];
        row_sum += diff * diff;
      }
    }
    sum += row_sum;
  }
  sum /= (xsize * ysize);
  return sqrt(sum);
}

float ButteraugliDistance(const extras::PackedPixelFile& a,
                          const extras::PackedPixelFile& b, ThreadPool* pool) {
  CodecInOut io0;
  JXL_CHECK(ConvertPackedPixelFileToCodecInOut(a, pool, &io0));
  CodecInOut io1;
  JXL_CHECK(ConvertPackedPixelFileToCodecInOut(b, pool, &io1));
  // TODO(eustas): simplify?
  return ButteraugliDistance(io0.frames[0], io1.frames[0], ButteraugliParams(),
                             *JxlGetDefaultCms(),
                             /*distmap=*/nullptr, pool);
}

float ButteraugliDistance(const ImageBundle& rgb0, const ImageBundle& rgb1,
                          const ButteraugliParams& params,
                          const JxlCmsInterface& cms, ImageF* distmap,
                          ThreadPool* pool, bool ignore_alpha) {
  float distance;
  JXL_CHECK(ComputeScore(rgb0, rgb1, params, cms, &distance, distmap, pool,
                         ignore_alpha));
  return distance;
}

float Butteraugli3Norm(const extras::PackedPixelFile& a,
                       const extras::PackedPixelFile& b, ThreadPool* pool) {
  CodecInOut io0;
  JXL_CHECK(ConvertPackedPixelFileToCodecInOut(a, pool, &io0));
  CodecInOut io1;
  JXL_CHECK(ConvertPackedPixelFileToCodecInOut(b, pool, &io1));
  ButteraugliParams ba;
  ImageF distmap;
  ButteraugliDistance(io0.frames[0], io1.frames[0], ba, *JxlGetDefaultCms(),
                      &distmap, pool);
  return ComputeDistanceP(distmap, ba, 3);
}

}  // namespace test

bool operator==(const jxl::Bytes& a, const jxl::Bytes& b) {
  if (a.size() != b.size()) return false;
  if (memcmp(a.data(), b.data(), a.size()) != 0) return false;
  return true;
}

// Allow using EXPECT_EQ on jxl::Bytes
bool operator!=(const jxl::Bytes& a, const jxl::Bytes& b) { return !(a == b); }

}  // namespace jxl
