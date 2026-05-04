// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "tools/no_memory_manager.h"

#include <cstdlib>

#include "lib/base/memory_manager.h"

namespace jpegli_tools {

namespace {
void* ToolsAlloc(void* /* opaque*/, size_t size) { return malloc(size); }
void ToolsFree(void* /* opaque*/, void* address) { free(address); }
JpegliMemoryManager kNoMemoryManager{nullptr, &ToolsAlloc, &ToolsFree};
}  // namespace

JpegliMemoryManager* NoMemoryManager() { return &kNoMemoryManager; };

}  // namespace jpegli_tools
