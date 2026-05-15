// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_LIB_BASE_C_CALLBACK_SUPPORT_H_
#define JPEGLI_LIB_BASE_C_CALLBACK_SUPPORT_H_

#include <utility>

namespace jpegli {
namespace detail {

template <typename T>
struct MethodToCCallbackHelper {};

template <typename T, typename R, typename... Args>
struct MethodToCCallbackHelper<R (T::*)(Args...)> {
  template <R (T::*method)(Args...)>
  static R Call(void *opaque, Args... args) {
    return (reinterpret_cast<T *>(opaque)->*method)(
        std::forward<Args>(args)...);
  }
};

}  // namespace detail
}  // namespace jpegli

#define METHOD_TO_C_CALLBACK(method) \
  ::jpegli::detail::MethodToCCallbackHelper<decltype(method)>::Call<method>

#endif  // JPEGLI_LIB_BASE_C_CALLBACK_SUPPORT_H_
