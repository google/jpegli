// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_EXTRAS_COMMON_H_
#define LIB_EXTRAS_COMMON_H_

#include <jxl/codestream_header.h>
#include <jxl/types.h>

#include <vector>

#include "lib/jxl/base/status.h"

namespace jxl {
namespace extras {

// TODO(sboukortt): consider exposing this as part of the C API.
Status SelectFormat(const std::vector<JxlPixelFormat>& accepted_formats,
                    const JxlBasicInfo& basic_info, JxlPixelFormat* format);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_COMMON_H_
