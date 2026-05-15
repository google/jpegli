// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_TOOLS_BENCHMARK_BENCHMARK_CODEC_JPEG_H_
#define JPEGLI_TOOLS_BENCHMARK_BENCHMARK_CODEC_JPEG_H_

#include "lib/base/status.h"
#include "tools/benchmark/benchmark_args.h"
#include "tools/benchmark/benchmark_codec.h"

namespace jpegli_tools {
ImageCodec* CreateNewJPEGCodec(const BenchmarkArgs& args);

// Registers the jpeg-specific command line options.
Status AddCommandLineOptionsJPEGCodec(BenchmarkArgs* args);
}  // namespace jpegli_tools

#endif  // JPEGLI_TOOLS_BENCHMARK_BENCHMARK_CODEC_JPEG_H_
