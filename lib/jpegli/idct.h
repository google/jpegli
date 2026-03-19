// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JPEGLI_IDCT_H_
#define LIB_JPEGLI_IDCT_H_

#include "lib/base/status.h"
#include "lib/jpegli/common.h"

namespace jpegli {

jxl::Status ChooseInverseTransform(j_decompress_ptr cinfo);

}  // namespace jpegli

#endif  // LIB_JPEGLI_IDCT_H_
