// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/base/rect.h"

#include <cstdint>

#include "lib/base/testing.h"

namespace jpegli {
namespace {

TEST(RectTest, IsSameRequiresEqualSize) {
  const RectT<uint64_t> base(0, 0, 16, 16);
  EXPECT_TRUE(base.IsSame(RectT<uint64_t>(0, 0, 16, 16)));
  EXPECT_FALSE(base.IsSame(RectT<uint64_t>(1, 0, 16, 16)));
  EXPECT_FALSE(base.IsSame(RectT<uint64_t>(0, 1, 16, 16)));
  EXPECT_FALSE(base.IsSame(RectT<uint64_t>(0, 0, 8, 16)));
  // A rect that is shorter or taller than `other` is not the same rect. The
  // height comparison must be an equality, not `<=`.
  EXPECT_FALSE(base.IsSame(RectT<uint64_t>(0, 0, 16, 8)));
  EXPECT_FALSE(base.IsSame(RectT<uint64_t>(0, 0, 16, 32)));
}

// Mirrors the APNG fcTL viewport check in third_party/apngdis/dec.cc: a frame
// whose viewport extends past the image bottom must not be treated as fully
// inside the image, otherwise later per-row copies read out of bounds.
TEST(RectTest, IntersectionIsSameRejectsOversizedHeight) {
  const RectT<uint64_t> image(0, 0, 16, 16);
  const RectT<uint64_t> inside(0, 0, 16, 16);
  const RectT<uint64_t> too_tall(0, 0, 16, 100);
  EXPECT_TRUE(inside.Intersection(image).IsSame(inside));
  EXPECT_FALSE(too_tall.Intersection(image).IsSame(too_tall));
}

}  // namespace
}  // namespace jpegli
