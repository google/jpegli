// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_TIME_H_
#define LIB_EXTRAS_TIME_H_

// OS-specific function for timing.

namespace jxl {

// Returns current time [seconds] from a monotonic clock with unspecified
// starting point - only suitable for computing elapsed time.
double Now();

}  // namespace jxl

#endif  // LIB_EXTRAS_TIME_H_
