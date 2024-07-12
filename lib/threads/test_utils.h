// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef LIB_THREADS_TEST_UTILS_H_
#define LIB_THREADS_TEST_UTILS_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

#include "lib/base/compiler_specific.h"
#include "lib/base/data_parallel.h"
#include "lib/threads/thread_parallel_runner_cxx.h"

namespace jxl {
namespace test {

class ThreadPoolForTests {
 public:
  explicit ThreadPoolForTests(int num_threads) {
    runner_ =
        JxlThreadParallelRunnerMake(/* memory_manager */ nullptr, num_threads);
    pool_ =
        jxl::make_unique<ThreadPool>(JxlThreadParallelRunner, runner_.get());
  }
  ThreadPoolForTests(const ThreadPoolForTests&) = delete;
  ThreadPoolForTests& operator&(const ThreadPoolForTests&) = delete;
  ThreadPool* get() { return pool_.get(); }

 private:
  JxlThreadParallelRunnerPtr runner_;
  std::unique_ptr<ThreadPool> pool_;
};

}  // namespace test
}  // namespace jxl

#endif  // LIB_THREADS_TEST_UTILS_H_
