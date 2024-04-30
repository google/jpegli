// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JPEGLI_ADAPTIVE_QUANTIZATION_H_
#define LIB_JPEGLI_ADAPTIVE_QUANTIZATION_H_

#include "lib/jpegli/common.h"

namespace jpegli {

void ComputeAdaptiveQuantField(j_compress_ptr cinfo);

}  // namespace jpegli

#endif  // LIB_JPEGLI_ADAPTIVE_QUANTIZATION_H_
