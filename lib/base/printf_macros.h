// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_LIB_BASE_PRINTF_MACROS_H_
#define JPEGLI_LIB_BASE_PRINTF_MACROS_H_

// Format string macros. These should be included after any other system
// library since those may unconditionally define these, depending on the
// platform.

// PRIuS and PRIdS macros to print size_t and ptrdiff_t respectively.
#if !defined(PRIdS)
#if defined(_WIN64)
#define PRIdS "lld"
#elif defined(_WIN32)
#define PRIdS "d"
#else
#define PRIdS "zd"
#endif
#endif  // PRIdS

#if !defined(PRIuS)
#if defined(_WIN64)
#define PRIuS "llu"
#elif defined(_WIN32)
#define PRIuS "u"
#else
#define PRIuS "zu"
#endif
#endif  // PRIuS

#endif  // JPEGLI_LIB_BASE_PRINTF_MACROS_H_
