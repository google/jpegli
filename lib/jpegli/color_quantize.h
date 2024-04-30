// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JPEGLI_COLOR_QUANTIZE_H_
#define LIB_JPEGLI_COLOR_QUANTIZE_H_

#include "lib/jpegli/common.h"

namespace jpegli {

void ChooseColorMap1Pass(j_decompress_ptr cinfo);

void ChooseColorMap2Pass(j_decompress_ptr cinfo);

void CreateInverseColorMap(j_decompress_ptr cinfo);

void CreateOrderedDitherTables(j_decompress_ptr cinfo);

void InitFSDitherState(j_decompress_ptr cinfo);

int LookupColorIndex(j_decompress_ptr cinfo, const JSAMPLE* pixel);

}  // namespace jpegli

#endif  // LIB_JPEGLI_COLOR_QUANTIZE_H_
