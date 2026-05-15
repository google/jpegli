// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_LIB_BASE_STATUS_H_
#define JPEGLI_LIB_BASE_STATUS_H_

// Error handling: Status return type + helper macros.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <utility>

#include "lib/base/common.h"
#include "lib/base/compiler_specific.h"

namespace jpegli {

// The Verbose level for the library
#ifndef JPEGLI_DEBUG_V_LEVEL
#define JPEGLI_DEBUG_V_LEVEL 0
#endif  // JPEGLI_DEBUG_V_LEVEL

#ifdef USE_ANDROID_LOGGER
#include <android/log.h>
#define LIBJPEGLI_ANDROID_LOG_TAG ("libjpegli")
inline void android_vprintf(const char* format, va_list args) {
  char* message = nullptr;
  int res = vasprintf(&message, format, args);
  if (res != -1) {
    __android_log_write(ANDROID_LOG_DEBUG, LIBJPEGLI_ANDROID_LOG_TAG, message);
    free(message);
  }
}
#endif

// Print a debug message on standard error or android logs. You should use the
// JPEGLI_DEBUG macro instead of calling Debug directly. This function returns
// false, so it can be used as a return value in JPEGLI_FAILURE.
JPEGLI_FORMAT(1, 2)
inline JPEGLI_NOINLINE bool Debug(const char* format, ...) {
  va_list args;
  va_start(args, format);
#ifdef USE_ANDROID_LOGGER
  android_vprintf(format, args);
#else
  vfprintf(stderr, format, args);
#endif
  va_end(args);
  return false;
}

// Print a debug message on standard error if "enabled" is true. "enabled" is
// normally a macro that evaluates to 0 or 1 at compile time, so the Debug
// function is never called and optimized out in release builds. Note that the
// arguments are compiled but not evaluated when enabled is false. The format
// string must be a explicit string in the call, for example:
//   JPEGLI_DEBUG(JPEGLI_DEBUG_MYMODULE, "my module message: %d", some_var);
// Add a header at the top of your module's .cc or .h file (depending on whether
// you have JPEGLI_DEBUG calls from the .h as well) like this:
//   #ifndef JPEGLI_DEBUG_MYMODULE
//   #define JPEGLI_DEBUG_MYMODULE 0
//   #endif JPEGLI_DEBUG_MYMODULE
#define JPEGLI_DEBUG_TMP(format, ...) \
  ::jpegli::Debug(("%s:%d: " format "\n"), __FILE__, __LINE__, ##__VA_ARGS__)

#define JPEGLI_DEBUG(enabled, format, ...)     \
  do {                                         \
    if (enabled) {                             \
      JPEGLI_DEBUG_TMP(format, ##__VA_ARGS__); \
    }                                          \
  } while (0)

// JPEGLI_DEBUG version that prints the debug message if the global verbose
// level defined at compile time by JPEGLI_DEBUG_V_LEVEL is greater or equal
// than the passed level.
#if JPEGLI_DEBUG_V_LEVEL > 0
#define JPEGLI_DEBUG_V(level, format, ...) \
  JPEGLI_DEBUG(level <= JPEGLI_DEBUG_V_LEVEL, format, ##__VA_ARGS__)
#else
#define JPEGLI_DEBUG_V(level, format, ...)
#endif

#define JPEGLI_WARNING(format, ...) \
  JPEGLI_DEBUG(JPEGLI_IS_DEBUG_BUILD, format, ##__VA_ARGS__)

#if JPEGLI_IS_DEBUG_BUILD
// Exits the program after printing a stack trace when possible.
JPEGLI_NORETURN inline JPEGLI_NOINLINE bool Abort() {
  JPEGLI_PRINT_STACK_TRACE();
  JPEGLI_CRASH();
}
#endif

#if JPEGLI_IS_DEBUG_BUILD
#define JPEGLI_DEBUG_ABORT(format, ...)                                      \
  do {                                                                       \
    if (JPEGLI_DEBUG_ON_ABORT) {                                             \
      ::jpegli::Debug(("%s:%d: JPEGLI_DEBUG_ABORT: " format "\n"), __FILE__, \
                      __LINE__, ##__VA_ARGS__);                              \
    }                                                                        \
    ::jpegli::Abort();                                                       \
  } while (0);
#else
#define JPEGLI_DEBUG_ABORT(format, ...)
#endif

// Use this for code paths that are unreachable unless the code would change
// to make it reachable, in which case it will print a warning and abort in
// debug builds. In release builds no code is produced for this, so only use
// this if this path is really unreachable.
#if JPEGLI_IS_DEBUG_BUILD
#define JPEGLI_UNREACHABLE(format, ...)                                   \
  (::jpegli::Debug(("%s:%d: JPEGLI_UNREACHABLE: " format "\n"), __FILE__, \
                   __LINE__, ##__VA_ARGS__),                              \
   ::jpegli::Abort(), JPEGLI_FAILURE(format, ##__VA_ARGS__))
#else  // JPEGLI_IS_DEBUG_BUILD
#define JPEGLI_UNREACHABLE(format, ...) \
  JPEGLI_FAILURE("internal: " format, ##__VA_ARGS__)
#endif

// Only runs in debug builds (builds where NDEBUG is not
// defined). This is useful for slower asserts that we want to run more rarely
// than usual. These will run on asan, msan and other debug builds, but not in
// opt or release.
#if JPEGLI_IS_DEBUG_BUILD
#define JPEGLI_DASSERT(condition)                                            \
  do {                                                                       \
    if (!(condition)) {                                                      \
      JPEGLI_DEBUG(JPEGLI_DEBUG_ON_ABORT, "JPEGLI_DASSERT: %s", #condition); \
      ::jpegli::Abort();                                                     \
    }                                                                        \
  } while (0)
#else
#define JPEGLI_DASSERT(condition)
#endif

// A jpegli::Status value from a StatusCode or Status which prints a debug
// message when enabled.
#define JPEGLI_STATUS(status, format, ...)                                 \
  ::jpegli::StatusMessage(::jpegli::Status(status), "%s:%d: " format "\n", \
                          __FILE__, __LINE__, ##__VA_ARGS__)

// Notify of an error but discard the resulting Status value. This is only
// useful for debug builds or when building with JPEGLI_CRASH_ON_ERROR.
#define JPEGLI_NOTIFY_ERROR(format, ...)                   \
  (void)JPEGLI_STATUS(::jpegli::StatusCode::kGenericError, \
                      "JPEGLI_ERROR: " format, ##__VA_ARGS__)

// An error Status with a message. The JPEGLI_STATUS() macro will return a
// Status object with a kGenericError code, but the comma operator helps with
// clang-tidy inference and potentially with optimizations.
#define JPEGLI_FAILURE(format, ...)                               \
  ((void)JPEGLI_STATUS(::jpegli::StatusCode::kGenericError,       \
                       "JPEGLI_FAILURE: " format, ##__VA_ARGS__), \
   ::jpegli::Status(::jpegli::StatusCode::kGenericError))

// Always evaluates the status exactly once, so can be used for non-debug calls.
// Returns from the current context if the passed Status expression is an error
// (fatal or non-fatal). The return value is the passed Status.
#define JPEGLI_RETURN_IF_ERROR(status)                                       \
  do {                                                                       \
    ::jpegli::Status jpegli_return_if_error_status = (status);               \
    if (!jpegli_return_if_error_status) {                                    \
      (void)::jpegli::StatusMessage(                                         \
          jpegli_return_if_error_status,                                     \
          "%s:%d: JPEGLI_RETURN_IF_ERROR code=%d: %s\n", __FILE__, __LINE__, \
          static_cast<int>(jpegli_return_if_error_status.code()), #status);  \
      return jpegli_return_if_error_status;                                  \
    }                                                                        \
  } while (0)

// As above, but without calling StatusMessage. Intended for bundles (see
// fields.h), which have numerous call sites (-> relevant for code size) and do
// not want to generate excessive messages when decoding partial headers.
#define JPEGLI_QUIET_RETURN_IF_ERROR(status)                   \
  do {                                                         \
    ::jpegli::Status jpegli_return_if_error_status = (status); \
    if (!jpegli_return_if_error_status) {                      \
      return jpegli_return_if_error_status;                    \
    }                                                          \
  } while (0)

#if JPEGLI_IS_DEBUG_BUILD
// Debug: fatal check.
#define JPEGLI_ENSURE(condition)                        \
  do {                                                  \
    if (!(condition)) {                                 \
      ::jpegli::Debug("JPEGLI_ENSURE: %s", #condition); \
      ::jpegli::Abort();                                \
    }                                                   \
  } while (0)
#else
// Release: non-fatal check of condition. If false, just return an error.
#define JPEGLI_ENSURE(condition)                              \
  do {                                                        \
    if (!(condition)) {                                       \
      return JPEGLI_FAILURE("JPEGLI_ENSURE: %s", #condition); \
    }                                                         \
  } while (0)
#endif

enum class StatusCode : int32_t {
  // Non-fatal errors (negative values).
  kNotEnoughBytes = -1,

  // The only non-error status code.
  kOk = 0,

  // Fatal-errors (positive values)
  kGenericError = 1,
};

// Drop-in replacement for bool that raises compiler warnings if not used
// after being returned from a function. Example:
// Status LoadFile(...) { return true; } is more compact than
// bool JPEGLI_MUST_USE_RESULT LoadFile(...) { return true; }
// In case of error, the status can carry an extra error code in its value which
// is split between fatal and non-fatal error codes.
class JPEGLI_MUST_USE_RESULT Status {
 public:
  // We want implicit constructor from bool to allow returning "true" or "false"
  // on a function when using Status. "true" means kOk while "false" means a
  // generic fatal error.
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Status(bool ok)
      : code_(ok ? StatusCode::kOk : StatusCode::kGenericError) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Status(StatusCode code) : code_(code) {}

  // We also want implicit cast to bool to check for return values of functions.
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator bool() const { return code_ == StatusCode::kOk; }

  constexpr StatusCode code() const { return code_; }

  // Returns whether the status code is a fatal error.
  constexpr bool IsFatalError() const {
    return static_cast<int32_t>(code_) > 0;
  }

 private:
  StatusCode code_;
};

static constexpr Status OkStatus() { return Status(StatusCode::kOk); }

// Helper function to create a Status and print the debug message or abort when
// needed.
inline JPEGLI_FORMAT(2, 3) Status
    StatusMessage(const Status status, const char* format, ...) {
  // This block will be optimized out when JPEGLI_IS_DEBUG_BUILD is disabled.
  if ((JPEGLI_IS_DEBUG_BUILD && status.IsFatalError()) ||
      (JPEGLI_DEBUG_ON_ALL_ERROR && !status)) {
    va_list args;
    va_start(args, format);
#ifdef USE_ANDROID_LOGGER
    android_vprintf(format, args);
#else
    vfprintf(stderr, format, args);
#endif
    va_end(args);
  }
#if JPEGLI_CRASH_ON_ERROR
  // JPEGLI_CRASH_ON_ERROR means to Abort() only on non-fatal errors.
  if (status.IsFatalError()) {
    ::jpegli::Abort();
  }
#endif  // JPEGLI_CRASH_ON_ERROR
  return status;
}

template <typename T>
class JPEGLI_MUST_USE_RESULT StatusOr {
  static_assert(!std::is_convertible<StatusCode, T>::value &&
                    !std::is_convertible<T, StatusCode>::value,
                "You cannot make a StatusOr with a type convertible from or to "
                "StatusCode");
  static_assert(std::is_move_constructible<T>::value &&
                    std::is_move_assignable<T>::value,
                "T must be move constructible and move assignable");

 public:
  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusOr(StatusCode code) : code_(code) {
    JPEGLI_DASSERT(code_ != StatusCode::kOk);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusOr(Status status) : StatusOr(status.code()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusOr(T&& value) : code_(StatusCode::kOk) {
    new (&storage_.data_) T(std::move(value));
  }

  StatusOr(StatusOr&& other) noexcept {
    if (other.ok()) {
      new (&storage_.data_) T(std::move(other.storage_.data_));
    }
    code_ = other.code_;
  }

  StatusOr& operator=(StatusOr&& other) noexcept {
    if (this == &other) return *this;
    if (ok() && other.ok()) {
      storage_.data_ = std::move(other.storage_.data_);
    } else if (other.ok()) {
      new (&storage_.data_) T(std::move(other.storage_.data_));
    } else if (ok()) {
      storage_.data_.~T();
    }
    code_ = other.code_;
    return *this;
  }

  StatusOr(const StatusOr&) = delete;
  StatusOr operator=(const StatusOr&) = delete;

  bool ok() const { return code_ == StatusCode::kOk; }
  Status status() const { return code_; }

  // Only call this if you are absolutely sure that `ok()` is true.
  // Never call this manually: rely on JPEGLI_ASSIGN_OR.
  T value_() && {
    JPEGLI_DASSERT(ok());
    return std::move(storage_.data_);
  }

  ~StatusOr() {
    if (code_ == StatusCode::kOk) {
      storage_.data_.~T();
    }
  }

 private:
  union Storage {
    char placeholder_;
    T data_;
    Storage() {}
    ~Storage() {}
  } storage_;

  StatusCode code_;
};

#define JPEGLI_ASSIGN_OR_RETURN(lhs, statusor)                         \
  PRIVATE_JPEGLI_ASSIGN_OR_RETURN_IMPL(                                \
      JPEGLI_JOIN(assign_or_return_temporary_variable, __LINE__), lhs, \
      statusor)

// NOLINTBEGIN(bugprone-macro-parentheses)
#define PRIVATE_JPEGLI_ASSIGN_OR_RETURN_IMPL(name, lhs, statusor) \
  auto name = statusor;                                           \
  JPEGLI_RETURN_IF_ERROR(name.status());                          \
  lhs = std::move(name).value_();
// NOLINTEND(bugprone-macro-parentheses)

#define JPEGLI_ASSIGN_OR_QUIT(lhs, statusor, message)                     \
  PRIVATE_JPEGLI_ASSIGN_OR_QUIT_IMPL(                                     \
      JPEGLI_JOIN(assign_or_temporary_variable, __LINE__), lhs, statusor, \
      message)

// NOLINTBEGIN(bugprone-macro-parentheses)
#define PRIVATE_JPEGLI_ASSIGN_OR_QUIT_IMPL(name, lhs, statusor, message) \
  auto name = statusor;                                                  \
  if (!name.ok()) {                                                      \
    QUIT(message);                                                       \
  }                                                                      \
  lhs = std::move(name).value_();
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace jpegli

#endif  // JPEGLI_LIB_BASE_STATUS_H_
