// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_LIB_BASE_OS_MACROS_H_
#define JPEGLI_LIB_BASE_OS_MACROS_H_

// Defines the JPEGLI_OS_* macros.

#if defined(_WIN32) || defined(_WIN64)
#define JPEGLI_OS_WIN 1
#else
#define JPEGLI_OS_WIN 0
#endif

#ifdef __linux__
#define JPEGLI_OS_LINUX 1
#else
#define JPEGLI_OS_LINUX 0
#endif

#ifdef __APPLE__
#define JPEGLI_OS_MAC 1
#else
#define JPEGLI_OS_MAC 0
#endif

#define JPEGLI_OS_IOS 0
#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#undef JPEGLI_OS_IOS
#define JPEGLI_OS_IOS 1
#endif
#endif

#ifdef __FreeBSD__
#define JPEGLI_OS_FREEBSD 1
#else
#define JPEGLI_OS_FREEBSD 0
#endif

#ifdef __HAIKU__
#define JPEGLI_OS_HAIKU 1
#else
#define JPEGLI_OS_HAIKU 0
#endif

#endif  // JPEGLI_LIB_BASE_OS_MACROS_H_
