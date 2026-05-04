// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef JPEGLI_TOOLS_TRACKING_MEMORY_MANAGER_H_
#define JPEGLI_TOOLS_TRACKING_MEMORY_MANAGER_H_

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <unordered_map>

#include "lib/base/memory_manager.h"
#include "lib/base/status.h"

namespace jpegli_tools {

const uint64_t kGiB = 1u << 30;

class TrackingMemoryManager {
 public:
  explicit TrackingMemoryManager(uint64_t cap = 0, uint64_t total_cap = 0);

  // void setInner(JpegliMemoryManager* inner) { inner_ = inner; }

  JpegliMemoryManager* get() { return &outer_; }

  jpegli::Status Reset();

  bool seen_oom = false;
  uint64_t max_bytes_in_use = 0;
  uint64_t total_allocations = 0;
  uint64_t total_bytes_allocated = 0;

 private:
  static void* Alloc(void* opaque, size_t size);
  static void Free(void* opaque, void* address);

  std::unordered_map<void*, size_t> allocations_;
  std::mutex numbers_mutex_;
  std::mutex map_mutex_;
  uint64_t cap_;
  uint64_t total_cap_;
  uint64_t bytes_in_use_ = 0;
  uint64_t num_allocations_ = 0;
  JpegliMemoryManager outer_;
  JpegliMemoryManager default_;
  JpegliMemoryManager* inner_;
};

}  // namespace jpegli_tools

#endif  // JPEGLI_TOOLS_TRACKING_MEMORY_MANAGER_H_
