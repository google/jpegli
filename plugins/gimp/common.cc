// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "plugins/gimp/common.h"

namespace jxl {

JpegXlGimpProgress::JpegXlGimpProgress(const char *message) {
  cur_progress = 0;
  max_progress = 100;

  gimp_progress_init_printf("%s\n", message);
}

void JpegXlGimpProgress::update() {
  gimp_progress_update(static_cast<float>(++cur_progress) /
                       static_cast<float>(max_progress));
}

void JpegXlGimpProgress::finished() { gimp_progress_update(1.0); }

}  // namespace jxl
