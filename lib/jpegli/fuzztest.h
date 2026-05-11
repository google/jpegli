// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef JPEGLI_LIB_JPEGLI_FUZZTEST_H_
#define JPEGLI_LIB_JPEGLI_FUZZTEST_H_

#include "lib/base/compiler_specific.h"

#if !defined(FUZZ_TEST)
struct FuzzTestSink {
  template <typename F>
  FuzzTestSink WithSeeds(F /*f*/) {
    return *this;
  }
};
#define FUZZ_TEST(A, B) \
  const JPEGLI_MAYBE_UNUSED FuzzTestSink unused##A##B = FuzzTestSink()
#endif

#endif  // JPEGLI_LIB_JPEGLI_FUZZTEST_H_
