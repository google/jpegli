// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JPEGLI_ENCODE_FINISH_H_
#define LIB_JPEGLI_ENCODE_FINISH_H_

#include "lib/jpegli/encode_internal.h"

namespace jpegli {

void QuantizetoPSNR(j_compress_ptr cinfo);

}  // namespace jpegli

#endif  // LIB_JPEGLI_ENCODE_FINISH_H_
