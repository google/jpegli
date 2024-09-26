// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_TEST_UTILS_H_
#define LIB_EXTRAS_TEST_UTILS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "lib/base/memory_manager.h"
#include "lib/extras/image.h"
#include "lib/extras/packed_image.h"

namespace jxl {
namespace test {

void Check(bool ok);

#define JXL_TEST_ASSIGN_OR_DIE(lhs, statusor) \
  PRIVATE_JXL_TEST_ASSIGN_OR_DIE_IMPL(        \
      JXL_JOIN(assign_or_die_temporary_variable, __LINE__), lhs, statusor)

// NOLINTBEGIN(bugprone-macro-parentheses)
#define PRIVATE_JXL_TEST_ASSIGN_OR_DIE_IMPL(name, lhs, statusor) \
  auto name = statusor;                                          \
  ::jxl::test::Check(name.ok());                                 \
  lhs = std::move(name).value_();
// NOLINTEND(bugprone-macro-parentheses)

std::vector<uint8_t> ReadTestData(const std::string& filename);

StatusOr<Image3F> GetColorImage(const extras::PackedPixelFile& ppf);

JxlMemoryManager* MemoryManager();

}  // namespace test
}  // namespace jxl

#endif  // LIB_EXTRAS_TEST_UTILS_H_
