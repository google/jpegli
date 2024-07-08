// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include "lib/extras/dec/decode.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "tools/file_io.h"
#include "tools/ssimulacra2.h"

int PrintUsage(char** argv) {
  fprintf(stderr, "Usage: %s orig.png distorted.png\n", argv[0]);
  fprintf(stderr,
          "Returns a score in range -inf..100, which correlates to subjective "
          "visual quality:\n");
  fprintf(stderr,
          "     30 = low quality (p10 worst output of mozjpeg -quality 30)\n");
  fprintf(stderr,
          "     50 = medium quality (average output of cjxl -q 40 or mozjpeg "
          "-quality 40,\n");
  fprintf(stderr,
          "                          p10 output of cjxl -q 50 or mozjpeg "
          "-quality 60)\n");
  fprintf(stderr,
          "     70 = high quality (average output of cjxl -q 70 or mozjpeg "
          "-quality 70,\n");
  fprintf(stderr,
          "                        p10 output of cjxl -q 75 or mozjpeg "
          "-quality 80)\n");
  fprintf(stderr,
          "     90 = very high quality (impossible to distinguish from "
          "original at 1:1,\n");
  fprintf(stderr,
          "                             average output of cjxl -q 90 or "
          "mozjpeg -quality 90)\n");
  return 1;
}

int main(int argc, char** argv) {
  if (argc != 3) return PrintUsage(argv);

  jxl::extras::PackedPixelFile ppf[2];
  const char* purpose[] = {"original", "distorted"};
  for (size_t i = 0; i < 2; ++i) {
    std::vector<uint8_t> encoded;
    if (!jpegxl::tools::ReadFile(argv[1 + i], &encoded)) {
      fprintf(stderr, "Could not load %s image: %s\n", purpose[i], argv[1 + i]);
      return 1;
    }
    if (!jxl::extras::DecodeBytes(
            jxl::Bytes(encoded), jxl::extras::ColorHints(),
            &ppf[i])) {
      fprintf(stderr, "Could not decode %s image: %s\n", purpose[i],
              argv[1 + i]);
      return 1;
    }
    if (ppf[i].xsize() < 8 || ppf[i].ysize() < 8) {
      fprintf(stderr, "Minimum image size is 8x8 pixels\n");
      return 1;
    }
  }
  jxl::extras::PackedPixelFile& ppf1 = ppf[0];
  jxl::extras::PackedPixelFile& ppf2 = ppf[1];

  if (ppf1.xsize() != ppf2.xsize() || ppf1.ysize() != ppf2.ysize()) {
    fprintf(stderr, "Image size mismatch\n");
    return 1;
  }

  jxl::StatusOr<Msssim> msssim_or = ComputeSSIMULACRA2(ppf1, ppf2);
  if (!msssim_or.ok()) {
    fprintf(stderr, "ComputeSSIMULACRA2 failed\n");
    return 1;
  }
  Msssim msssim = std::move(msssim_or).value();
  printf("%.8f\n", msssim.Score());
  return 0;
}
