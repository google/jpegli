// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/test_memory_manager.h"

#include <cstdlib>

#include "lib/base/memory_manager.h"

namespace jpegli {
namespace test {

namespace {
void* TestAlloc(void* /* opaque*/, size_t size) { return malloc(size); }
void TestFree(void* /* opaque*/, void* address) { free(address); }
JpegliMemoryManager kMemoryManager{nullptr, &TestAlloc, &TestFree};
}  // namespace

JpegliMemoryManager* MemoryManager() { return &kMemoryManager; };

}  // namespace test
}  // namespace jpegli
