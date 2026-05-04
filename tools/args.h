// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef JPEGLI_TOOLS_ARGS_H_
#define JPEGLI_TOOLS_ARGS_H_

// Helpers for parsing command line arguments. No include guard needed.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "lib/base/override.h"
#include "lib/base/status.h"
#include "lib/extras/dec/color_hints.h"
#include "tools/file_io.h"

namespace jpegli_tools {

static inline bool ParseOverride(const char* arg, jpegli::Override* out) {
  const std::string s_arg(arg);
  if (s_arg == "1") {
    *out = jpegli::Override::kOn;
    return true;
  }
  if (s_arg == "0") {
    *out = jpegli::Override::kOff;
    return true;
  }
  fprintf(stderr, "Invalid flag, %s must be 0 or 1\n", arg);
  return JPEGLI_FAILURE("Args");
}

template <typename Callback>
static inline bool ParseAndAppendKeyValue(const char* arg, Callback* cb) {
  const char* eq = strchr(arg, '=');
  if (!eq) {
    fprintf(stderr, "Expected argument as 'key=value' but received '%s'\n",
            arg);
    return false;
  }
  std::string key(arg, eq);
  return (*cb)(key, std::string(eq + 1));
}

static inline bool ParseCString(const char* arg, const char** out) {
  *out = arg;
  return true;
}

static inline bool IncrementUnsigned(size_t* out) {
  (*out)++;
  return true;
}

struct ColorHintsProxy {
  jpegli::extras::ColorHints target;
  bool operator()(const std::string& key, const std::string& value) {
    if (key == "icc_pathname") {
      std::vector<uint8_t> icc;
      JPEGLI_RETURN_IF_ERROR(ReadFile(value, &icc));
      const char* data = reinterpret_cast<const char*>(icc.data());
      target.Add("icc", std::string(data, data + icc.size()));
    } else if (key == "exif" || key == "xmp" || key == "jumbf") {
      std::vector<uint8_t> metadata;
      JPEGLI_RETURN_IF_ERROR(ReadFile(value, &metadata));
      const char* data = reinterpret_cast<const char*>(metadata.data());
      target.Add(key, std::string(data, data + metadata.size()));
    } else if (key == "strip") {
      target.Add(value, "");
    } else {
      target.Add(key, value);
    }
    return true;
  }
};

}  // namespace jpegli_tools

#endif  // JPEGLI_TOOLS_ARGS_H_
