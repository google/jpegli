// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/extras/test_utils.h"

#include <cstddef>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lib/base/compiler_specific.h"
#include "lib/base/status.h"
#include "lib/base/types.h"
#include "lib/extras/image.h"
#include "lib/extras/packed_image.h"
#include "lib/extras/packed_image_convert.h"

#if !defined(TEST_DATA_PATH)
#include "tools/cpp/runfiles/runfiles.h"
#endif

namespace jxl {
namespace test {

void Check(bool ok) {
  if (!ok) {
    JXL_CRASH();
  }
}

#if defined(TEST_DATA_PATH)
std::string GetTestDataPath(const std::string& filename) {
  return std::string(TEST_DATA_PATH "/") + filename;
}
#else
using bazel::tools::cpp::runfiles::Runfiles;
const std::unique_ptr<Runfiles> kRunfiles(Runfiles::Create(""));
std::string GetTestDataPath(const std::string& filename) {
  std::string root(JPEGLI_ROOT_PACKAGE "/testdata/");
  return kRunfiles->Rlocation(root + filename);
}
#endif

std::vector<uint8_t> ReadTestData(const std::string& filename) {
  std::string full_path = GetTestDataPath(filename);
  fprintf(stderr, "ReadTestData %s\n", full_path.c_str());
  std::ifstream file(full_path, std::ios::binary);
  std::vector<char> str((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
  Check(file.good());
  const uint8_t* raw = reinterpret_cast<const uint8_t*>(str.data());
  std::vector<uint8_t> data(raw, raw + str.size());
  printf("Test data %s is %d bytes long.\n", filename.c_str(),
         static_cast<int>(data.size()));
  return data;
}

StatusOr<Image3F> GetColorImage(const extras::PackedPixelFile& ppf) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  JXL_ENSURE(!ppf.frames.empty());
  JXL_TEST_ASSIGN_OR_DIE(
      Image3F color, Image3F::Create(memory_manager, ppf.xsize(), ppf.ysize()));
  JXL_ENSURE(ConvertPackedPixelFileToImage3F(ppf, &color, nullptr));
  return color;
}

}  // namespace test
}  // namespace jxl
