// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_LIB_BASE_COMPILER_SPECIFIC_H_
#define JPEGLI_LIB_BASE_COMPILER_SPECIFIC_H_

// Macros for compiler version + nonstandard keywords, e.g. __builtin_expect.

#include <sys/types.h>  // IWYU pragma: export
#ifdef __clang_analyzer__
#include <stdio.h>  // IWYU pragma: export
#endif

#include "lib/base/sanitizer_definitions.h"

#if JPEGLI_ADDRESS_SANITIZER || JPEGLI_MEMORY_SANITIZER || \
    JPEGLI_THREAD_SANITIZER
#include "sanitizer/common_interface_defs.h"  // __sanitizer_print_stack_trace
#endif                                        // defined(*_SANITIZER)

// #if is shorter and safer than #ifdef. *_VERSION are zero if not detected,
// otherwise 100 * major + minor version. Note that other packages check for
// #ifdef COMPILER_MSVC, so we cannot use that same name.

#ifdef _MSC_VER
#define JPEGLI_COMPILER_MSVC _MSC_VER
#else
#define JPEGLI_COMPILER_MSVC 0
#endif

#ifdef __GNUC__
#define JPEGLI_COMPILER_GCC (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#define JPEGLI_COMPILER_GCC 0
#endif

#ifdef __clang__
#define JPEGLI_COMPILER_CLANG (__clang_major__ * 100 + __clang_minor__)
// Clang pretends to be GCC for compatibility.
#undef JPEGLI_COMPILER_GCC
#define JPEGLI_COMPILER_GCC 0
#else
#define JPEGLI_COMPILER_CLANG 0
#endif

#if JPEGLI_COMPILER_MSVC
#define JPEGLI_RESTRICT __restrict
#elif JPEGLI_COMPILER_GCC || JPEGLI_COMPILER_CLANG
#define JPEGLI_RESTRICT __restrict__
#else
#define JPEGLI_RESTRICT
#endif

#if JPEGLI_COMPILER_MSVC
#define JPEGLI_INLINE __forceinline
#define JPEGLI_NOINLINE __declspec(noinline)
#else
#define JPEGLI_INLINE inline __attribute__((always_inline))
#define JPEGLI_NOINLINE __attribute__((noinline))
#endif

#if JPEGLI_COMPILER_MSVC
#define JPEGLI_NORETURN __declspec(noreturn)
#elif JPEGLI_COMPILER_GCC || JPEGLI_COMPILER_CLANG
#define JPEGLI_NORETURN __attribute__((noreturn))
#else
#define JPEGLI_NORETURN
#endif

#if JPEGLI_COMPILER_MSVC
#define JPEGLI_MAYBE_UNUSED
#else
// Encountered "attribute list cannot appear here" when using the C++17
// [[maybe_unused]], so only use the old style attribute for now.
#define JPEGLI_MAYBE_UNUSED __attribute__((unused))
#endif

// MSAN execution won't hurt if some code it not inlined, but this can greatly
// improve compilation time. Unfortunately this macro can not be used just
// everywhere - inside header files it leads to "multiple definition" error;
// though it would be better not to have JPEGLI_INLINE in header overall.
#if JPEGLI_MEMORY_SANITIZER || JPEGLI_ADDRESS_SANITIZER || \
    JPEGLI_THREAD_SANITIZER
#define JPEGLI_MAYBE_INLINE JPEGLI_MAYBE_UNUSED
#else
#define JPEGLI_MAYBE_INLINE JPEGLI_INLINE
#endif

#if JPEGLI_COMPILER_MSVC
// Unsupported, __assume is not the same.
#define JPEGLI_LIKELY(expr) expr
#define JPEGLI_UNLIKELY(expr) expr
#else
#define JPEGLI_LIKELY(expr) __builtin_expect(!!(expr), 1)
#define JPEGLI_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#endif

// Returns a void* pointer which the compiler then assumes is N-byte aligned.
// Example: float* JPEGLI_RESTRICT aligned = (float*)JPEGLI_ASSUME_ALIGNED(in,
// 32);
//
// The assignment semantics are required by GCC/Clang. ICC provides an in-place
// __assume_aligned, whereas MSVC's __assume appears unsuitable.
#if JPEGLI_COMPILER_CLANG
// Early versions of Clang did not support __builtin_assume_aligned.
#define JPEGLI_HAS_ASSUME_ALIGNED __has_builtin(__builtin_assume_aligned)
#elif JPEGLI_COMPILER_GCC
#define JPEGLI_HAS_ASSUME_ALIGNED 1
#else
#define JPEGLI_HAS_ASSUME_ALIGNED 0
#endif

#if JPEGLI_HAS_ASSUME_ALIGNED
#define JPEGLI_ASSUME_ALIGNED(ptr, align) \
  __builtin_assume_aligned((ptr), (align))
#else
#define JPEGLI_ASSUME_ALIGNED(ptr, align) (ptr) /* not supported */
#endif

#ifdef __has_attribute
#define JPEGLI_HAVE_ATTRIBUTE(x) __has_attribute(x)
#else
#define JPEGLI_HAVE_ATTRIBUTE(x) 0
#endif

// Raises warnings if the function return value is unused. Should appear as the
// first part of a function definition/declaration.
#if JPEGLI_HAVE_ATTRIBUTE(nodiscard)
#define JPEGLI_MUST_USE_RESULT [[nodiscard]]
#elif JPEGLI_COMPILER_CLANG && JPEGLI_HAVE_ATTRIBUTE(warn_unused_result)
#define JPEGLI_MUST_USE_RESULT __attribute__((warn_unused_result))
#else
#define JPEGLI_MUST_USE_RESULT
#endif

// Disable certain -fsanitize flags for functions that are expected to include
// things like unsigned integer overflow. For example use in the function
// declaration JPEGLI_NO_SANITIZE("unsigned-integer-overflow") to silence
// unsigned integer overflow ubsan messages.
#if JPEGLI_COMPILER_CLANG && JPEGLI_HAVE_ATTRIBUTE(no_sanitize)
#define JPEGLI_NO_SANITIZE(X) __attribute__((no_sanitize(X)))
#else
#define JPEGLI_NO_SANITIZE(X)
#endif

#if JPEGLI_HAVE_ATTRIBUTE(__format__)
#define JPEGLI_FORMAT(idx_fmt, idx_arg) \
  __attribute__((__format__(__printf__, idx_fmt, idx_arg)))
#else
#define JPEGLI_FORMAT(idx_fmt, idx_arg)
#endif

// C++ standard.
#if defined(_MSC_VER) && !defined(__clang__) && defined(_MSVC_LANG) && \
    _MSVC_LANG > __cplusplus
#define JPEGLI_CXX_LANG _MSVC_LANG
#else
#define JPEGLI_CXX_LANG __cplusplus
#endif

// Known / distinguished C++ standards.
#define JPEGLI_CXX_17 201703

// In most cases we consider build as "debug". Use `NDEBUG` for release build.
#if defined(JPEGLI_IS_DEBUG_BUILD)
#undef JPEGLI_IS_DEBUG_BUILD
#define JPEGLI_IS_DEBUG_BUILD 1
#elif defined(NDEBUG)
#define JPEGLI_IS_DEBUG_BUILD 0
#else
#define JPEGLI_IS_DEBUG_BUILD 1
#endif

#if defined(JPEGLI_CRASH_ON_ERROR)
#undef JPEGLI_CRASH_ON_ERROR
#define JPEGLI_CRASH_ON_ERROR 1
#else
#define JPEGLI_CRASH_ON_ERROR 0
#endif

#if JPEGLI_CRASH_ON_ERROR && !JPEGLI_IS_DEBUG_BUILD
#error "JPEGLI_CRASH_ON_ERROR requires JPEGLI_IS_DEBUG_BUILD"
#endif

// Pass -DJPEGLI_DEBUG_ON_ALL_ERROR at compile time to print debug messages on
// all error (fatal and non-fatal) status.
#if defined(JPEGLI_DEBUG_ON_ALL_ERROR)
#undef JPEGLI_DEBUG_ON_ALL_ERROR
#define JPEGLI_DEBUG_ON_ALL_ERROR 1
#else
#define JPEGLI_DEBUG_ON_ALL_ERROR 0
#endif

#if JPEGLI_DEBUG_ON_ALL_ERROR && !JPEGLI_IS_DEBUG_BUILD
#error "JPEGLI_DEBUG_ON_ALL_ERROR requires JPEGLI_IS_DEBUG_BUILD"
#endif

// Pass -DJPEGLI_DEBUG_ON_ABORT={0} to disable the debug messages on
// (debug) JPEGLI_ENSURE and JPEGLI_DASSERT.
#if !defined(JPEGLI_DEBUG_ON_ABORT)
#define JPEGLI_DEBUG_ON_ABORT JPEGLI_IS_DEBUG_BUILD
#endif  // JPEGLI_DEBUG_ON_ABORT

#if JPEGLI_DEBUG_ON_ABORT && !JPEGLI_IS_DEBUG_BUILD
#error "JPEGLI_DEBUG_ON_ABORT requires JPEGLI_IS_DEBUG_BUILD"
#endif

#if JPEGLI_ADDRESS_SANITIZER || JPEGLI_MEMORY_SANITIZER || \
    JPEGLI_THREAD_SANITIZER
#define JPEGLI_PRINT_STACK_TRACE() __sanitizer_print_stack_trace();
#else
#define JPEGLI_PRINT_STACK_TRACE()
#endif

#if JPEGLI_COMPILER_MSVC
#define JPEGLI_CRASH() __debugbreak(), (void)abort()
#else
#define JPEGLI_CRASH() (void)__builtin_trap()
#endif

#endif  // JPEGLI_LIB_BASE_COMPILER_SPECIFIC_H_
