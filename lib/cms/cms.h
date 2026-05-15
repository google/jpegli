// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_CMS_H_
#define JPEGLI_CMS_H_

// ICC profiles and color space conversions.

#include <jpegli/jpegli_cms_export.h>

#include "lib/cms/cms_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

JPEGLI_CMS_EXPORT const JpegliCmsInterface* JpegliGetDefaultCms();

#ifdef __cplusplus
}
#endif

#endif  // JPEGLI_CMS_H_
