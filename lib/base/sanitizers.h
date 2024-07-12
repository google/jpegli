// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JXL_BASE_SANITIZERS_H_
#define LIB_JXL_BASE_SANITIZERS_H_

#include <cstddef>

#include "lib/base/compiler_specific.h"
#include "lib/base/sanitizer_definitions.h"

#if JXL_MEMORY_SANITIZER
#include "sanitizer/msan_interface.h"
#endif

namespace jxl {
namespace msan {

// Chosen so that kSanitizerSentinel is four copies of kSanitizerSentinelByte.
constexpr uint8_t kSanitizerSentinelByte = 0x48;
constexpr float kSanitizerSentinel = 205089.125f;

#if JXL_MEMORY_SANITIZER

static JXL_INLINE JXL_MAYBE_UNUSED void PoisonMemory(const volatile void* m,
                                                     size_t size) {
  __msan_poison(m, size);
}

static JXL_INLINE JXL_MAYBE_UNUSED void UnpoisonMemory(const volatile void* m,
                                                       size_t size) {
  __msan_unpoison(m, size);
}

static JXL_INLINE JXL_MAYBE_UNUSED void MemoryIsInitialized(
    const volatile void* m, size_t size) {
  __msan_check_mem_is_initialized(m, size);
}

#else  // JXL_MEMORY_SANITIZER

// In non-msan mode these functions don't use volatile since it is not needed
// for the empty functions.

static JXL_INLINE JXL_MAYBE_UNUSED void PoisonMemory(const void* m,
                                                     size_t size) {}
static JXL_INLINE JXL_MAYBE_UNUSED void UnpoisonMemory(const void* m,
                                                       size_t size) {}
static JXL_INLINE JXL_MAYBE_UNUSED void MemoryIsInitialized(const void* m,
                                                            size_t size) {}

#endif

}  // namespace msan
}  // namespace jxl

#endif  // LIB_JXL_BASE_SANITIZERS_H_
