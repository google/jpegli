// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/extras/image.h"

#include <algorithm>  // fill, swap
#include <cstddef>
#include <cstdint>

#include "lib/base/memory_manager.h"
#include "lib/base/status.h"
#include "lib/extras/cache_aligned.h"
#include "lib/extras/simd_util.h"

#if defined(MEMORY_SANITIZER)
#include "lib/base/common.h"
#include "lib/base/sanitizers.h"
#endif

namespace jxl {
namespace detail {

namespace {

size_t BytesPerRow(const size_t xsize, const size_t sizeof_t) {
  // Special case: we don't allow any ops -> don't need extra padding/
  if (xsize == 0) {
    return 0;
  }

  const size_t vec_size = MaxVectorSize();
  size_t valid_bytes = xsize * sizeof_t;

  // Allow unaligned accesses starting at the last valid value.
  // Skip for the scalar case because no extra lanes will be loaded.
  if (vec_size != 0) {
    valid_bytes += vec_size - sizeof_t;
  }

  // Round up to vector and cache line size.
  const size_t align = std::max(vec_size, CacheAligned::kAlignment);
  size_t bytes_per_row = RoundUpTo(valid_bytes, align);

  // During the lengthy window before writes are committed to memory, CPUs
  // guard against read after write hazards by checking the address, but
  // only the lower 11 bits. We avoid a false dependency between writes to
  // consecutive rows by ensuring their sizes are not multiples of 2 KiB.
  // Avoid2K prevents the same problem for the planes of an Image3.
  if (bytes_per_row % CacheAligned::kAlias == 0) {
    bytes_per_row += align;
  }

  JXL_ASSERT(bytes_per_row % align == 0);
  return bytes_per_row;
}

// Initializes the minimum bytes required to suppress MSAN warnings from
// legitimate vector loads/stores on the right border, where some lanes are
// uninitialized and assumed to be unused.
void InitializePadding(PlaneBase& plane, const size_t sizeof_t) {
#if defined(MEMORY_SANITIZER)
  size_t xsize = plane.xsize();
  size_t ysize = plane.ysize();
  if (xsize == 0 || ysize == 0) return;

  const size_t vec_size = MaxVectorSize();
  if (vec_size == 0) return;  // Scalar mode: no padding needed

  const size_t valid_size = xsize * sizeof_t;
  const size_t initialize_size = RoundUpTo(valid_size, vec_size);
  if (valid_size == initialize_size) return;

  for (size_t y = 0; y < ysize; ++y) {
    uint8_t* JXL_RESTRICT row = plane.bytes() + y * plane.bytes_per_row();
#if defined(__clang__) &&                                           \
    ((!defined(__apple_build_version__) && __clang_major__ <= 6) || \
     (defined(__apple_build_version__) &&                           \
      __apple_build_version__ <= 10001145))
    // There's a bug in MSAN in clang-6 when handling AVX2 operations. This
    // workaround allows tests to pass on MSAN, although it is slower and
    // prevents MSAN warnings from uninitialized images.
    std::fill(row, msan::kSanitizerSentinelByte, initialize_size);
#else
    memset(row + valid_size, msan::kSanitizerSentinelByte,
           initialize_size - valid_size);
#endif  // clang6
  }
#endif  // MEMORY_SANITIZER
}

}  // namespace

PlaneBase::PlaneBase(const size_t xsize, const size_t ysize,
                     const size_t sizeof_t)
    : xsize_(static_cast<uint32_t>(xsize)),
      ysize_(static_cast<uint32_t>(ysize)),
      orig_xsize_(static_cast<uint32_t>(xsize)),
      orig_ysize_(static_cast<uint32_t>(ysize)),
      bytes_per_row_(BytesPerRow(xsize_, sizeof_t)),
      memory_manager_(nullptr),
      bytes_(nullptr),
      sizeof_t_(sizeof_t) {
  // TODO(eustas): turn to error instead of abort.
  JXL_CHECK(xsize == xsize_);
  JXL_CHECK(ysize == ysize_);

  JXL_ASSERT(sizeof_t == 1 || sizeof_t == 2 || sizeof_t == 4 || sizeof_t == 8);
}

Status PlaneBase::Allocate(JxlMemoryManager* memory_manager) {
  JXL_CHECK(memory_manager_ == nullptr);
  JXL_CHECK(memory_manager != nullptr);
  JXL_CHECK(!bytes_.get());

  // Dimensions can be zero, e.g. for lazily-allocated images. Only allocate
  // if nonzero, because "zero" bytes still have padding/bookkeeping overhead.
  if (xsize_ == 0 || ysize_ == 0) {
    return true;
  }

  bytes_ = AllocateArray(bytes_per_row_ * ysize_);
  if (!bytes_.get()) {
    // TODO(eustas): use specialized OOM error code
    return JXL_FAILURE("Failed to allocate memory for image surface");
  }
  memory_manager_ = memory_manager;
  InitializePadding(*this, sizeof_t_);

  return true;
}

void PlaneBase::Swap(PlaneBase& other) {
  std::swap(xsize_, other.xsize_);
  std::swap(ysize_, other.ysize_);
  std::swap(orig_xsize_, other.orig_xsize_);
  std::swap(orig_ysize_, other.orig_ysize_);
  std::swap(bytes_per_row_, other.bytes_per_row_);
  std::swap(memory_manager_, other.memory_manager_);
  std::swap(bytes_, other.bytes_);
}

}  // namespace detail
}  // namespace jxl
