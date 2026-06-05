// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_LIB_BASE_INCLUDE_JPEGLIB_H_
#define JPEGLI_LIB_BASE_INCLUDE_JPEGLIB_H_

// Using this header ensures that includes go in the right order,
// not alphabetically sorted.

// NOLINTBEGIN
/* clang-format off */
#include <stdio.h>  // IWYU pragma: keep

#ifdef _WIN32
#undef RIGHT_SHIFT_IS_UNSIGNED
/* Define "boolean" as unsigned char, not int, per Windows custom */
#ifndef __RPCNDR_H__
typedef unsigned char boolean;
#endif
#define HAVE_BOOLEAN  /* prevent jmorecfg.h from redefining it */
/* Define "INT32" as int, not long, per Windows custom */
#if !(defined(_BASETSD_H_) || defined(_BASETSD_H))
typedef short INT16;
typedef signed int INT32;
#endif
#define XMD_H  /* prevent jmorecfg.h from redefining it */
#endif

#include <jpeglib.h>  // IWYU pragma: export
#include <setjmp.h>  // IWYU pragma: export
/* clang-format on */
// NOLINTEND

#endif  // JPEGLI_LIB_BASE_INCLUDE_JPEGLIB_H_
