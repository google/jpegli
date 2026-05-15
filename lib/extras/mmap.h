// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_LIB_EXTRAS_MMAP_H_
#define JPEGLI_LIB_EXTRAS_MMAP_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "lib/base/status.h"

namespace jpegli {
struct MemoryMappedFileImpl;

class MemoryMappedFile {
 public:
  static StatusOr<MemoryMappedFile> Init(const char* path);
  const uint8_t* data() const;
  size_t size() const;
  MemoryMappedFile();                                        // NOLINT
  ~MemoryMappedFile();                                       // NOLINT
  MemoryMappedFile(MemoryMappedFile&&) noexcept;             // NOLINT
  MemoryMappedFile& operator=(MemoryMappedFile&&) noexcept;  // NOLINT

 private:
  std::unique_ptr<MemoryMappedFileImpl> impl_;
};
}  // namespace jpegli

#endif
