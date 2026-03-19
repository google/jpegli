// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "lib/base/span.h"
#include "lib/base/status.h"
#include "lib/extras/dec/color_hints.h"
#include "lib/extras/dec/decode.h"
#include "lib/extras/packed_image.h"
#include "tools/file_io.h"
#include "tools/ssimulacra2.h"

#define QUIT(M)               \
  fprintf(stderr, "%s\n", M); \
  return EXIT_FAILURE;

int PrintUsage(char** argv) {
  fprintf(stderr, "Usage: %s original.png distorted.png\n", argv[0]);
  fprintf(stderr,
          "Returns a score in range -inf..100, which correlates to subjective "
          "visual quality:\n");
  fprintf(
      stderr,
      "     negative scores: extremely low quality, very strong distortion\n");
  fprintf(stderr,
          "     10 = very low quality (average output of cjxl -d 14 / -q 12 or "
          "libjpeg-turbo quality 14)\n");
  fprintf(stderr,
          "     30 = low quality (average output of cjxl -d 9 / -q 20 or "
          "libjpeg-turbo quality 20)\n");
  fprintf(stderr,
          "     50 = medium quality (average output of cjxl -d 5 / -q 45 or "
          "libjpeg-turbo quality 35)\n");
  fprintf(stderr,
          "     70 = high quality (hard to notice artifacts without comparison "
          "to the original,\n");
  fprintf(stderr,
          "                        average output of cjxl -d 2.5 / -q 73 or "
          "libjpeg-turbo quality 70)\n");
  fprintf(stderr,
          "     80 = very high quality (impossible to distinguish from the "
          "original in a side-by-side comparison at 1:1,\n");
  fprintf(stderr,
          "                             average output of cjxl -d 1.5 / -q 85 "
          "or libjpeg-turbo quality 85 (4:2:2))\n");
  fprintf(stderr,
          "     85 = excellent quality (impossible to distinguish from the "
          "original in a flip test at 1:1,\n");
  fprintf(stderr,
          "                             average output of cjxl -d 1 / -q 90 or "
          "libjpeg-turbo quality 90 (4:4:4))\n");
  fprintf(stderr,
          "     90 = visually lossless (impossible to distinguish from the "
          "original in a flicker test at 1:1,\n");

  fprintf(stderr,
          "                             average output of cjxl -d 0.5 / -q 95 "
          "or libjpeg-turbo quality 95 (4:4:4)\n");
  fprintf(stderr, "     100 = mathematically lossless\n");

  return 1;
}

int main(int argc, char** argv) {
  if (argc != 3) return PrintUsage(argv);

  std::vector<jxl::extras::PackedPixelFile> ppf(2);
  const char* purpose[] = {"original", "distorted"};
  for (size_t i = 0; i < 2; ++i) {
    std::vector<uint8_t> encoded;
    if (!jpegxl::tools::ReadFile(argv[1 + i], &encoded)) {
      fprintf(stderr, "Could not load %s image: %s\n", purpose[i], argv[1 + i]);
      return 1;
    }
    if (!jxl::extras::DecodeBytes(jxl::Bytes(encoded),
                                  jxl::extras::ColorHints(), &ppf[i])) {
      fprintf(stderr, "Could not decode %s image: %s\n", purpose[i],
              argv[1 + i]);
      return 1;
    }
    if (ppf[i].xsize() < 8 || ppf[i].ysize() < 8) {
      QUIT("Minimum image size is 8x8 pixels\n");
    }
  }
  jxl::extras::PackedPixelFile& ppf1 = ppf[0];
  jxl::extras::PackedPixelFile& ppf2 = ppf[1];

  if (ppf1.xsize() != ppf2.xsize() || ppf1.ysize() != ppf2.ysize()) {
    QUIT("Image size mismatch\n");
  }

  JXL_ASSIGN_OR_QUIT(Msssim msssim, ComputeSSIMULACRA2(ppf1, ppf2),
                     "ComputeSSIMULACRA2 failed.");
  printf("%.8f\n", msssim.Score());
  return EXIT_SUCCESS;
}
