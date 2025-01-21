// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_JXL_BASE_TESTING_H_
#define LIB_JXL_BASE_TESTING_H_

// GTest specific macros / wrappers.

#include "gtest/gtest.h"  // IWYU pragma: export

#ifdef JXL_DISABLE_SLOW_TESTS
#define JXL_SLOW_TEST(T, C) TEST(T, DISABLED_##C)
#else
#define JXL_SLOW_TEST(T, C) TEST(T, C)
#endif  // JXL_DISABLE_SLOW_TESTS

#ifdef THREAD_SANITIZER
#define JXL_TSAN_SLOW_TEST(T, C) TEST(T, DISABLED_##C)
#else
#define JXL_TSAN_SLOW_TEST(T, C) TEST(T, C)
#endif  // THREAD_SANITIZER

// googletest before 1.10 didn't define INSTANTIATE_TEST_SUITE_P() but instead
// used INSTANTIATE_TEST_CASE_P which is now deprecated.
#ifdef INSTANTIATE_TEST_SUITE_P
#define JXL_GTEST_INSTANTIATE_TEST_SUITE_P INSTANTIATE_TEST_SUITE_P
#else
#define JXL_GTEST_INSTANTIATE_TEST_SUITE_P INSTANTIATE_TEST_CASE_P
#endif

// Ensures that we don't make our test bounds too lax, effectively disabling the
// tests.
#define EXPECT_SLIGHTLY_BELOW(A, E)       \
  {                                       \
    double _actual = (A);                 \
    double _expected = (E);               \
    EXPECT_LE(_actual, _expected);        \
    EXPECT_GE(_actual, 0.75 * _expected); \
  }

#define EXPECT_ARRAY_NEAR(A, E, T)                                         \
  {                                                                        \
    const auto _actual = (A);                                              \
    const auto _expected = (E);                                            \
    const auto _tolerance = (T);                                           \
    size_t _n = _expected.size();                                          \
    ASSERT_EQ(_actual.size(), _n);                                         \
    for (size_t _i = 0; _i < _n; ++_i) {                                   \
      EXPECT_NEAR(_actual[_i], _expected[_i], _tolerance)                  \
          << "@" << _i << ": " << _actual[_i] << " !~= " << _expected[_i]; \
    }                                                                      \
  }

#define JXL_EXPECT_OK(F)       \
  {                            \
    std::stringstream _;       \
    EXPECT_TRUE(F) << _.str(); \
  }

#define JXL_TEST_ASSERT_OK(F)  \
  {                            \
    std::stringstream _;       \
    ASSERT_TRUE(F) << _.str(); \
  }

#define QUIT(M) FAIL() << M;

#endif  // LIB_JXL_BASE_TESTING_H_
