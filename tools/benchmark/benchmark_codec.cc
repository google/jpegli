// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "tools/benchmark/benchmark_codec.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "lib/base/common.h"
#include "lib/base/memory_manager.h"
#include "lib/base/status.h"
#include "lib/base/types.h"
#include "lib/extras/image.h"
#include "lib/extras/image_ops.h"
#include "lib/extras/packed_image_convert.h"
#include "lib/extras/time.h"
#include "tools/benchmark/benchmark_args.h"
#include "tools/benchmark/benchmark_codec_jpeg.h"
#include "tools/benchmark/benchmark_stats.h"
#include "tools/cmdline.h"
#include "tools/no_memory_manager.h"
#include "tools/speed_stats.h"
#include "tools/thread_pool_internal.h"

namespace jpegxl {
namespace tools {

using ::jxl::Image3F;
using ::jxl::Status;

Status ImageCodec::ParseParameters(const std::string& parameters) {
  params_ = parameters;
  std::vector<std::string> parts = SplitString(parameters, ':');
  for (const auto& part : parts) {
    if (!ParseParam(part)) {
      return JXL_FAILURE("Invalid parameter %s", part.c_str());
    }
  }
  return true;
}

Status ImageCodec::ParseParam(const std::string& param) {
  if (param[0] == 'q') {  // libjpeg-style quality, [0,100]
    const std::string quality_param = param.substr(1);
    char* end;
    const float q_target = strtof(quality_param.c_str(), &end);
    if (end == quality_param.c_str() ||
        end != quality_param.c_str() + quality_param.size()) {
      return false;
    }
    q_target_ = q_target;
    return true;
  }
  if (param[0] == 'd') {  // butteraugli distance
    const std::string distance_param = param.substr(1);
    char* end;
    const float butteraugli_target = strtof(distance_param.c_str(), &end);
    if (end == distance_param.c_str() ||
        end != distance_param.c_str() + distance_param.size()) {
      return false;
    }
    butteraugli_target_ = butteraugli_target;
    return true;
  } else if (param[0] == 'r') {
    bitrate_target_ = strtof(param.substr(1).c_str(), nullptr);
    return true;
  }
  return false;
}

ImageCodecPtr CreateImageCodec(const std::string& description,
                               JxlMemoryManager* memory_manager) {
  std::string name = description;
  std::string parameters;
  size_t colon = description.find(':');
  if (colon < description.size()) {
    name = description.substr(0, colon);
    parameters = description.substr(colon + 1);
  }
  ImageCodecPtr result;
  if (name == "jpeg") {
    result.reset(CreateNewJPEGCodec(*Args()));
  }
  if (!result.get()) {
    fprintf(stderr, "Unknown image codec: %s", name.c_str());
    JPEGXL_TOOLS_CHECK(false);
  }
  result->set_description(description);
  if (!parameters.empty()) {
    JPEGXL_TOOLS_CHECK(result->ParseParameters(parameters));
  }
  return result;
}

}  // namespace tools
}  // namespace jpegxl
