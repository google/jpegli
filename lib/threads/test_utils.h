// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_LIB_THREADS_TEST_UTILS_H_
#define JPEGLI_LIB_THREADS_TEST_UTILS_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

#include "lib/base/compiler_specific.h"
#include "lib/base/data_parallel.h"
#include "lib/threads/thread_parallel_runner_cxx.h"

namespace jpegli {
namespace test {

class ThreadPoolForTests {
 public:
  explicit ThreadPoolForTests(int num_threads) {
    runner_ = JpegliThreadParallelRunnerMake(/* memory_manager */ nullptr,
                                             num_threads);
    pool_ = jpegli::make_unique<ThreadPool>(JpegliThreadParallelRunner,
                                            runner_.get());
  }
  ThreadPoolForTests(const ThreadPoolForTests&) = delete;
  ThreadPoolForTests& operator&(const ThreadPoolForTests&) = delete;
  ThreadPool* get() { return pool_.get(); }

 private:
  JpegliThreadParallelRunnerPtr runner_;
  std::unique_ptr<ThreadPool> pool_;
};

}  // namespace test
}  // namespace jpegli

#endif  // JPEGLI_LIB_THREADS_TEST_UTILS_H_
