/* Copyright (c) the JPEG XL Project Authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://developers.google.com/open-source/licenses/bsd
 */

/** Implementation of JpegliParallelRunner than can be used to enable
 * multithreading when using the JPEGLI library. This uses std::thread
 * internally and related synchronization functions. The number of threads
 * created is fixed at construction time and the threads are re-used for every
 * ThreadParallelRunner::Runner call. Only one concurrent
 * JpegliThreadParallelRunner call per instance is allowed at a time.
 *
 * This is a scalable, lower-overhead thread pool runner, especially suitable
 * for data-parallel computations in the fork-join model, where clients need to
 * know when all tasks have completed.
 *
 * This thread pool can efficiently load-balance millions of tasks using an
 * atomic counter, thus avoiding per-task virtual or system calls. With 48
 * hyperthreads and 1M tasks that add to an atomic counter, overall runtime is
 * 10-20x higher when using std::async, and ~200x for a queue-based thread
 */

#ifndef JPEGLI_THREAD_PARALLEL_RUNNER_H_
#define JPEGLI_THREAD_PARALLEL_RUNNER_H_

#include <jpegli/jpegli_threads_export.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "lib/base/memory_manager.h"
#include "lib/base/parallel_runner.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Parallel runner internally using std::thread. Use as @ref
 * JpegliParallelRunner.
 */
JPEGLI_THREADS_EXPORT JpegliParallelRetCode JpegliThreadParallelRunner(
    void* runner_opaque, void* jpegli_opaque, JpegliParallelRunInit init,
    JpegliParallelRunFunction func, uint32_t start_range, uint32_t end_range);

/** Creates the runner for @ref JpegliThreadParallelRunner. Use as the opaque
 * runner.
 */
JPEGLI_THREADS_EXPORT void* JpegliThreadParallelRunnerCreate(
    const JpegliMemoryManager* memory_manager, size_t num_worker_threads);

/** Destroys the runner created by @ref JpegliThreadParallelRunnerCreate.
 */
JPEGLI_THREADS_EXPORT void JpegliThreadParallelRunnerDestroy(
    void* runner_opaque);

/** Returns a default num_worker_threads value for
 * @ref JpegliThreadParallelRunnerCreate.
 */
JPEGLI_THREADS_EXPORT size_t
JpegliThreadParallelRunnerDefaultNumWorkerThreads(void);

#ifdef __cplusplus
}
#endif

#endif /* JPEGLI_THREAD_PARALLEL_RUNNER_H_ */
