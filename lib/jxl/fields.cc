// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/jxl/fields.h"

#include <algorithm>
#include <cinttypes>  // PRIu64
#include <cmath>
#include <cstddef>
#include <hwy/base.h>

#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/printf_macros.h"

namespace jxl {

namespace {

using ::jxl::fields_internal::VisitorBase;

struct InitVisitor : public VisitorBase {
  Status Bits(const size_t /*unused*/, const uint32_t default_value,
              uint32_t* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status U32(const U32Enc /*unused*/, const uint32_t default_value,
             uint32_t* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status U64(const uint64_t default_value,
             uint64_t* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status Bool(bool default_value, bool* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status F16(const float default_value, float* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  // Always visit conditional fields to ensure they are initialized.
  Status Conditional(bool /*condition*/) override { return true; }

  Status AllDefault(const Fields& /*fields*/,
                    bool* JXL_RESTRICT all_default) override {
    // Just initialize this field and don't skip initializing others.
    JXL_RETURN_IF_ERROR(Bool(true, all_default));
    return false;
  }

  Status VisitNested(Fields* /*fields*/) override {
    // Avoid re-initializing nested bundles (their ctors already called
    // Bundle::Init for their fields).
    return true;
  }
};

// Similar to InitVisitor, but also initializes nested fields.
struct SetDefaultVisitor : public VisitorBase {
  Status Bits(const size_t /*unused*/, const uint32_t default_value,
              uint32_t* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status U32(const U32Enc /*unused*/, const uint32_t default_value,
             uint32_t* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status U64(const uint64_t default_value,
             uint64_t* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status Bool(bool default_value, bool* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  Status F16(const float default_value, float* JXL_RESTRICT value) override {
    *value = default_value;
    return true;
  }

  // Always visit conditional fields to ensure they are initialized.
  Status Conditional(bool /*condition*/) override { return true; }

  Status AllDefault(const Fields& /*fields*/,
                    bool* JXL_RESTRICT all_default) override {
    // Just initialize this field and don't skip initializing others.
    JXL_RETURN_IF_ERROR(Bool(true, all_default));
    return false;
  }
};

class AllDefaultVisitor : public VisitorBase {
 public:
  explicit AllDefaultVisitor() = default;

  Status Bits(const size_t bits, const uint32_t default_value,
              uint32_t* JXL_RESTRICT value) override {
    all_default_ &= *value == default_value;
    return true;
  }

  Status U32(const U32Enc /*unused*/, const uint32_t default_value,
             uint32_t* JXL_RESTRICT value) override {
    all_default_ &= *value == default_value;
    return true;
  }

  Status U64(const uint64_t default_value,
             uint64_t* JXL_RESTRICT value) override {
    all_default_ &= *value == default_value;
    return true;
  }

  Status F16(const float default_value, float* JXL_RESTRICT value) override {
    all_default_ &= std::abs(*value - default_value) < 1E-6f;
    return true;
  }

  Status AllDefault(const Fields& /*fields*/,
                    bool* JXL_RESTRICT /*all_default*/) override {
    // Visit all fields so we can compute the actual all_default_ value.
    return false;
  }

  bool AllDefault() const { return all_default_; }

 private:
  bool all_default_ = true;
};

}  // namespace

void Bundle::Init(Fields* fields) {
  InitVisitor visitor;
  if (!visitor.Visit(fields)) {
    JXL_UNREACHABLE("Init should never fail");
  }
}
void Bundle::SetDefault(Fields* fields) {
  SetDefaultVisitor visitor;
  if (!visitor.Visit(fields)) {
    JXL_UNREACHABLE("SetDefault should never fail");
  }
}
bool Bundle::AllDefault(const Fields& fields) {
  AllDefaultVisitor visitor;
  if (!visitor.VisitConst(fields)) {
    JXL_UNREACHABLE("AllDefault should never fail");
  }
  return visitor.AllDefault();
}

}  // namespace jxl
