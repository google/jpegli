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

namespace jxl {
namespace test {

std::vector<uint8_t> ReadTestData(const std::string& filename);

JxlMemoryManager* MemoryManager();

}  // namespace test
}  // namespace jxl

#endif  // LIB_EXTRAS_TEST_UTILS_H_
