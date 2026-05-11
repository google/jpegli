// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Helper class for storing external (int or float, interleaved) images. This is
// the common format used by other libraries and in the libjpegli API.

#include "packed_image.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lib/base/byte_order.h"
#include "lib/base/common.h"
#include "lib/base/status.h"
#include "lib/base/types.h"
#include "lib/extras/codestream_header.h"

namespace jpegli {
namespace extras {

// Class representing an interleaved image with a bunch of channels.
StatusOr<PackedImage> PackedImage::Create(size_t xsize, size_t ysize,
                                          const JpegliPixelFormat& format) {
  JPEGLI_ASSIGN_OR_RETURN(size_t stride, CalcStride(format, xsize));
  size_t pixels_size = ysize * stride;
  if ((pixels_size / stride) != ysize) {
    return JPEGLI_FAILURE("Image too big");
  }
  PackedImage image(xsize, ysize, format, stride);
  if (!image.pixels()) {
    // TODO(szabadka): use specialized OOM error code
    return JPEGLI_FAILURE("Failed to allocate memory for image");
  }
  return image;
}

StatusOr<PackedImage> PackedImage::Copy() const {
  // Resulting copy_stride have to be less or equal to original -> always ok.
  JPEGLI_ASSIGN_OR_RETURN(size_t copy_stride, CalcStride(format, xsize));
  PackedImage copy(xsize, ysize, format, copy_stride);
  const uint8_t* orig_pixels = reinterpret_cast<const uint8_t*>(pixels());
  uint8_t* copy_pixels = reinterpret_cast<uint8_t*>(copy.pixels());
  if (stride == copy_stride) {
    // Same stride -> copy in one go.
    memcpy(copy_pixels, orig_pixels, ysize * stride);
  } else {
    // Otherwise, copy row-wise.
    JPEGLI_DASSERT(copy_stride < stride);
    for (size_t y = 0; y < ysize; ++y) {
      memcpy(copy_pixels + y * copy_stride, orig_pixels + y * stride,
             copy_stride);
    }
  }
  return copy;
}

Status PackedImage::ValidateDataType(JpegliDataType data_type) {
  if ((data_type != JPEGLI_TYPE_UINT8) && (data_type != JPEGLI_TYPE_UINT16) &&
      (data_type != JPEGLI_TYPE_FLOAT) && (data_type != JPEGLI_TYPE_FLOAT16)) {
    return JPEGLI_FAILURE("Unhandled data type: %d",
                          static_cast<int>(data_type));
  }
  return true;
}

size_t PackedImage::BitsPerChannel(JpegliDataType data_type) {
  switch (data_type) {
    case JPEGLI_TYPE_UINT8:
      return 8;
    case JPEGLI_TYPE_UINT16:
      return 16;
    case JPEGLI_TYPE_FLOAT:
      return 32;
    case JPEGLI_TYPE_FLOAT16:
      return 16;
    default:
      JPEGLI_DEBUG_ABORT("Unreachable");
      return 0;
  }
}

// Logical resize; use Copy() for storage reallocation, if necessary.
Status PackedImage::ShrinkTo(size_t new_xsize, size_t new_ysize) {
  if (new_xsize > xsize || new_ysize > ysize) {
    return JPEGLI_FAILURE("Cannot shrink PackedImage to a larger size");
  }
  xsize = new_xsize;
  ysize = new_ysize;
  return true;
}

PackedImage::PackedImage(size_t xsize, size_t ysize,
                         const JpegliPixelFormat& format, size_t stride)
    : xsize(xsize),
      ysize(ysize),
      stride(stride),
      format(format),
      pixels_size(ysize * stride),
      pixels_(malloc(std::max<size_t>(1, pixels_size)), free) {
  bytes_per_channel_ = BitsPerChannel(format.data_type) / jpegli::kBitsPerByte;
  pixel_stride_ = format.num_channels * bytes_per_channel_;
  swap_endianness_ = SwapEndianness(format.endianness);
}

StatusOr<size_t> PackedImage::CalcStride(const JpegliPixelFormat& format,
                                         size_t xsize) {
  size_t multiplier = (BitsPerChannel(format.data_type) * format.num_channels /
                       jpegli::kBitsPerByte);
  size_t stride = xsize * multiplier;
  if ((stride / multiplier) != xsize) {
    return JPEGLI_FAILURE("Image too big");
  }
  if (format.align > 1) {
    size_t aligned_stride =
        jpegli::DivCeil(stride, format.align) * format.align;
    if (stride > aligned_stride) {
      return JPEGLI_FAILURE("Image too big");
    }
    stride = aligned_stride;
  }
  return stride;
}

PackedFrame::PackedFrame(PackedImage&& image) : color(std::move(image)) {}

PackedFrame::PackedFrame(PackedFrame&& other) = default;

PackedFrame& PackedFrame::operator=(PackedFrame&& other) = default;

PackedFrame::~PackedFrame() = default;

StatusOr<PackedFrame> PackedFrame::Create(size_t xsize, size_t ysize,
                                          const JpegliPixelFormat& format) {
  JPEGLI_ASSIGN_OR_RETURN(PackedImage image,
                          PackedImage::Create(xsize, ysize, format));
  PackedFrame frame(std::move(image));
  return frame;
}

StatusOr<PackedFrame> PackedFrame::Copy() const {
  JPEGLI_ASSIGN_OR_RETURN(
      PackedFrame copy,
      PackedFrame::Create(color.xsize, color.ysize, color.format));
  copy.frame_info = frame_info;
  copy.name = name;
  JPEGLI_ASSIGN_OR_RETURN(copy.color, color.Copy());
  for (const auto& ec : extra_channels) {
    JPEGLI_ASSIGN_OR_RETURN(PackedImage ec_copy, ec.Copy());
    copy.extra_channels.emplace_back(std::move(ec_copy));
  }
  return copy;
}

// Logical resize; use Copy() for storage reallocation, if necessary.
Status PackedFrame::ShrinkTo(size_t new_xsize, size_t new_ysize) {
  JPEGLI_RETURN_IF_ERROR(color.ShrinkTo(new_xsize, new_ysize));
  for (auto& ec : extra_channels) {
    JPEGLI_RETURN_IF_ERROR(ec.ShrinkTo(new_xsize, new_ysize));
  }
  frame_info.layer_info.xsize = new_xsize;
  frame_info.layer_info.ysize = new_ysize;
  return true;
}

PackedPixelFile::PackedPixelFile() { JpegliEncoderInitBasicInfo(&info); };

Status PackedPixelFile::ShrinkTo(size_t new_xsize, size_t new_ysize) {
  for (auto& frame : frames) {
    JPEGLI_RETURN_IF_ERROR(frame.ShrinkTo(new_xsize, new_ysize));
  }
  info.xsize = new_xsize;
  info.ysize = new_ysize;
  return true;
}

}  // namespace extras
}  // namespace jpegli
