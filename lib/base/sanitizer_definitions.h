// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_LIB_BASE_SANITIZER_DEFINITIONS_H_
#define JPEGLI_LIB_BASE_SANITIZER_DEFINITIONS_H_

#ifdef MEMORY_SANITIZER
#define JPEGLI_MEMORY_SANITIZER 1
#elif defined(__has_feature)
#if __has_feature(memory_sanitizer)
#define JPEGLI_MEMORY_SANITIZER 1
#else
#define JPEGLI_MEMORY_SANITIZER 0
#endif
#else
#define JPEGLI_MEMORY_SANITIZER 0
#endif

#ifdef ADDRESS_SANITIZER
#define JPEGLI_ADDRESS_SANITIZER 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define JPEGLI_ADDRESS_SANITIZER 1
#else
#define JPEGLI_ADDRESS_SANITIZER 0
#endif
#else
#define JPEGLI_ADDRESS_SANITIZER 0
#endif

#ifdef THREAD_SANITIZER
#define JPEGLI_THREAD_SANITIZER 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define JPEGLI_THREAD_SANITIZER 1
#else
#define JPEGLI_THREAD_SANITIZER 0
#endif
#else
#define JPEGLI_THREAD_SANITIZER 0
#endif
#endif  // JPEGLI_LIB_BASE_SANITIZER_DEFINITIONS_H
