// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/extras/packed_image_convert.h"

#include <cstdint>
#include <cstdio>

#include "lib/extras/packed_image.h"
#include "lib/base/rect.h"
#include "lib/base/status.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/dec_external_image.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/enc_image_bundle.h"
#include "lib/jxl/luminance.h"

namespace jxl {
namespace extras {

Status ConvertPackedFrameToImageBundle(const JxlBasicInfo& info,
                                       const JxlBitDepth& input_bitdepth,
                                       const PackedFrame& frame,
                                       const CodecInOut& io, ThreadPool* pool,
                                       ImageBundle* bundle) {
  JxlMemoryManager* memory_manager = io.memory_manager;
  JXL_ENSURE(frame.color.pixels() != nullptr);
  size_t frame_bits_per_sample;
  if (input_bitdepth.type == JXL_BIT_DEPTH_FROM_PIXEL_FORMAT) {
    JXL_RETURN_IF_ERROR(
        PackedImage::ValidateDataType(frame.color.format.data_type));
    frame_bits_per_sample =
        PackedImage::BitsPerChannel(frame.color.format.data_type);
  } else {
    frame_bits_per_sample = info.bits_per_sample;
  }
  JXL_ENSURE(frame_bits_per_sample != 0);
  // It is ok for the frame.color.format.num_channels to not match the
  // number of channels on the image.
  JXL_ENSURE(1 <= frame.color.format.num_channels &&
             frame.color.format.num_channels <= 4);

  const Span<const uint8_t> span(
      static_cast<const uint8_t*>(frame.color.pixels()),
      frame.color.pixels_size);
  JXL_ENSURE(Rect(frame.frame_info.layer_info.crop_x0,
                  frame.frame_info.layer_info.crop_y0,
                  frame.frame_info.layer_info.xsize,
                  frame.frame_info.layer_info.ysize)
                 .IsInside(Rect(0, 0, info.xsize, info.ysize)));
  if (info.have_animation) {
    bundle->duration = frame.frame_info.duration;
    bundle->blend = frame.frame_info.layer_info.blend_info.blendmode > 0;
    bundle->use_for_next_frame =
        frame.frame_info.layer_info.save_as_reference > 0;
    bundle->origin.x0 = frame.frame_info.layer_info.crop_x0;
    bundle->origin.y0 = frame.frame_info.layer_info.crop_y0;
  }
  bundle->name = frame.name;  // frame.frame_info.name_length is ignored here.
  JXL_ENSURE(io.metadata.m.color_encoding.IsGray() ==
             (frame.color.format.num_channels <= 2));

template <void(StoreFunc)(float, uint8_t*)>
void StoreFloatRow(const float* JXL_RESTRICT* rows_in, size_t num_channels,
                   size_t xsize, uint8_t* JXL_RESTRICT out) {
  for (size_t x = 0; x < xsize; ++x) {
    for (size_t c = 0; c < num_channels; c++) {
      StoreFunc(rows_in[c][x], out + (num_channels * x + c) * sizeof(float));
    }
  }
}

  bundle->extra_channels().resize(io.metadata.m.extra_channel_info.size());
  for (size_t i = 0; i < frame.extra_channels.size(); i++) {
    const auto& ppf_ec = frame.extra_channels[i];
    JXL_ASSIGN_OR_RETURN(
        bundle->extra_channels()[i],
        ImageF::Create(memory_manager, ppf_ec.xsize, ppf_ec.ysize));
    JXL_RETURN_IF_ERROR(BufferToImageF(
        ppf_ec.format, ppf_ec.xsize, ppf_ec.ysize, ppf_ec.pixels(),
        ppf_ec.pixels_size, pool, &bundle->extra_channels()[i]));
  }
  return true;
}

Status ConvertPackedPixelFileToCodecInOut(const PackedPixelFile& ppf,
                                          ThreadPool* pool, CodecInOut* io) {
  JxlMemoryManager* memory_manager = io->memory_manager;
  const bool has_alpha = ppf.info.alpha_bits != 0;
  JXL_ENSURE(!ppf.frames.empty());
  if (has_alpha) {
    JXL_ENSURE(ppf.info.alpha_bits == ppf.info.bits_per_sample);
    JXL_ENSURE(ppf.info.alpha_exponent_bits ==
               ppf.info.exponent_bits_per_sample);
  }

  const bool is_gray = (ppf.info.num_color_channels == 1);
  JXL_ENSURE(ppf.info.num_color_channels == 1 ||
             ppf.info.num_color_channels == 3);

  // Convert the image metadata
  JXL_RETURN_IF_ERROR(io->SetSize(ppf.info.xsize, ppf.info.ysize));
  io->metadata.m.bit_depth.bits_per_sample = ppf.info.bits_per_sample;
  io->metadata.m.bit_depth.exponent_bits_per_sample =
      ppf.info.exponent_bits_per_sample;
  io->metadata.m.bit_depth.floating_point_sample =
      ppf.info.exponent_bits_per_sample != 0;
  io->metadata.m.modular_16_bit_buffer_sufficient =
      ppf.info.exponent_bits_per_sample == 0 && ppf.info.bits_per_sample <= 12;

  const bool little_endian =
      format.endianness == JXL_LITTLE_ENDIAN ||
      (format.endianness == JXL_NATIVE_ENDIAN && IsLittleEndian());

  io->metadata.m.xyb_encoded = !FROM_JXL_BOOL(ppf.info.uses_original_profile);
  JXL_ENSURE(ppf.info.orientation > 0 && ppf.info.orientation <= 8);
  io->metadata.m.orientation = ppf.info.orientation;

  // Convert animation metadata
  JXL_ENSURE(ppf.frames.size() == 1 || ppf.info.have_animation);
  io->metadata.m.have_animation = FROM_JXL_BOOL(ppf.info.have_animation);
  io->metadata.m.animation.tps_numerator = ppf.info.animation.tps_numerator;
  io->metadata.m.animation.tps_denominator = ppf.info.animation.tps_denominator;
  io->metadata.m.animation.num_loops = ppf.info.animation.num_loops;

  if (error_count) {
    JXL_FAILURE("unsupported pixel format data type");
  }

  return true;
}

Status ConvertFromExternal(const uint8_t* data, size_t size, size_t xsize,
                           size_t ysize, size_t bits_per_sample,
                           JxlPixelFormat format, size_t c, ThreadPool* pool,
                           ImageF* channel) {
  size_t bytes_per_channel = JxlDataTypeBytes(format.data_type);
  size_t bytes_per_pixel = format.num_channels * bytes_per_channel;
  const size_t last_row_size = xsize * bytes_per_pixel;
  const size_t align = format.align;
  const size_t row_size =
      (align > 1 ? jxl::DivCeil(last_row_size, align) * align : last_row_size);
  const size_t bytes_to_read = row_size * (ysize - 1) + last_row_size;
  if (xsize == 0 || ysize == 0) return JXL_FAILURE("Empty image");
  if (size > 0 && size < bytes_to_read) {
    return JXL_FAILURE("Buffer size is too small, expected: %" PRIuS
                       " got: %" PRIuS " (Image: %" PRIuS "x%" PRIuS
                       "x%u, bytes_per_channel: %" PRIuS ")",
                       bytes_to_read, size, xsize, ysize, format.num_channels,
                       bytes_per_channel);
  }
  // Too large buffer is likely an application bug, so also fail for that.
  // Do allow padding to stride in last row though.
  if (size > row_size * ysize) {
    return JXL_FAILURE("Buffer size is too large");
  }
  return ConvertFromExternalNoSizeCheck(
      data, xsize, ysize, row_size, bits_per_sample, format, c, pool, channel);
}

Status GetColorEncoding(const PackedPixelFile& ppf,
                        ColorEncoding* color_encoding) {
  if (ppf.primary_color_representation == PackedPixelFile::kIccIsPrimary) {
    const bool is_gray = (ppf.info.num_color_channels == 1);
    IccBytes icc = ppf.icc;
    if (!color_encoding->SetICC(std::move(icc), JxlGetDefaultCms())) {
      fprintf(stderr, "Warning: error setting ICC profile, assuming SRGB\n");
      *color_encoding = ColorEncoding::SRGB(is_gray);
    } else {
      if (color_encoding->IsCMYK()) {
        // We expect gray or tri-color.
        return JXL_FAILURE("Embedded ICC is CMYK");
      }
      if (color_encoding->IsGray() != is_gray) {
        // E.g. JPG image has 3 channels, but gray ICC.
        return JXL_FAILURE("Embedded ICC does not match image color type");
      }
    }
  } else {
    JXL_RETURN_IF_ERROR(color_encoding->FromExternal(ppf.color_encoding));
    if (color_encoding->ICC().empty()) {
      return JXL_FAILURE("Failed to serialize ICC");
    }
  }
  return true;
}

float GetIntensityTarget(const extras::PackedPixelFile& ppf,
                         const ColorEncoding& c_enc) {
  if (ppf.info.intensity_target != 0) {
    return ppf.info.intensity_target;
  } else if (c_enc.Tf().IsPQ()) {
    // Peak luminance of PQ as defined by SMPTE ST 2084:2014.
    return 10000;
  } else if (c_enc.Tf().IsHLG()) {
    // Nominal display peak luminance used as a reference by
    // Rec. ITU-R BT.2100-2.
    return 1000;
  } else {
    // SDR
    return kDefaultIntensityTarget;
  }
}

Status ConvertPackedPixelFileToImage3F(const extras::PackedPixelFile& ppf,
                                       Image3F* color, ThreadPool* pool) {
  JXL_RETURN_IF_ERROR(!ppf.frames.empty());
  const extras::PackedImage& img = ppf.frames[0].color;
  size_t bits_per_sample =
      ppf.input_bitdepth.type == JXL_BIT_DEPTH_FROM_PIXEL_FORMAT
          ? extras::PackedImage::BitsPerChannel(img.format.data_type)
          : ppf.info.bits_per_sample;
  for (size_t c = 0; c < ppf.info.num_color_channels; ++c) {
    JXL_RETURN_IF_ERROR(ConvertFromExternal(
        reinterpret_cast<const uint8_t*>(img.pixels()), img.pixels_size,
        img.xsize, img.ysize, bits_per_sample, img.format, c, pool,
        &color->Plane(c)));
  }
  if (ppf.info.num_color_channels == 1) {
    CopyImageTo(color->Plane(0), &color->Plane(1));
    CopyImageTo(color->Plane(0), &color->Plane(2));
  }
  return true;
}

StatusOr<PackedPixelFile> ConvertImage3FToPackedPixelFile(
    const Image3F& image, const ColorEncoding& c_enc, JxlPixelFormat format,
    ThreadPool* pool) {
  PackedPixelFile ppf{};
  ppf.info.xsize = image.xsize();
  ppf.info.ysize = image.ysize();
  ppf.info.num_color_channels = 3;
  JXL_RETURN_IF_ERROR(PackedImage::ValidateDataType(format.data_type));
  ppf.info.bits_per_sample = PackedImage::BitsPerChannel(format.data_type);
  ppf.info.exponent_bits_per_sample = format.data_type == JXL_TYPE_FLOAT ? 8
                                      : format.data_type == JXL_TYPE_FLOAT16
                                          ? 5
                                          : 0;
  ppf.color_encoding = c_enc.ToExternal();
  ppf.frames.clear();
  JXL_ASSIGN_OR_RETURN(
      PackedFrame frame,
      PackedFrame::Create(image.xsize(), image.ysize(), format));
  const ImageF* channels[3];
  for (int c = 0; c < 3; ++c) {
    channels[c] = &image.Plane(c);
  }
  bool float_samples = ppf.info.exponent_bits_per_sample > 0;
  JXL_RETURN_IF_ERROR(ConvertChannelsToExternal(
      channels, 3, ppf.info.bits_per_sample, float_samples, format.endianness,
      frame.color.stride, pool, frame.color.pixels(0, 0, 0),
      frame.color.pixels_size));
  ppf.frames.emplace_back(std::move(frame));
  return ppf;
}

// Allows converting from internal CodecInOut to external PackedPixelFile
Status ConvertCodecInOutToPackedPixelFile(const CodecInOut& io,
                                          const JxlPixelFormat& pixel_format,
                                          const ColorEncoding& c_desired,
                                          ThreadPool* pool,
                                          PackedPixelFile* ppf) {
  JxlMemoryManager* memory_manager = io.memory_manager;
  const bool has_alpha = io.metadata.m.HasAlpha();
  JXL_ENSURE(!io.frames.empty());

  if (has_alpha) {
    JXL_ENSURE(io.metadata.m.GetAlphaBits() ==
               io.metadata.m.bit_depth.bits_per_sample);
    const auto* alpha_channel = io.metadata.m.Find(ExtraChannel::kAlpha);
    JXL_ENSURE(alpha_channel->bit_depth.exponent_bits_per_sample ==
               io.metadata.m.bit_depth.exponent_bits_per_sample);
    ppf->info.alpha_bits = alpha_channel->bit_depth.bits_per_sample;
    ppf->info.alpha_exponent_bits =
        alpha_channel->bit_depth.exponent_bits_per_sample;
    ppf->info.alpha_premultiplied =
        TO_JXL_BOOL(alpha_channel->alpha_associated);
  }

  // Convert the image metadata
  ppf->info.xsize = io.metadata.size.xsize();
  ppf->info.ysize = io.metadata.size.ysize();
  ppf->info.num_color_channels = io.metadata.m.color_encoding.Channels();
  ppf->info.bits_per_sample = io.metadata.m.bit_depth.bits_per_sample;
  ppf->info.exponent_bits_per_sample =
      io.metadata.m.bit_depth.exponent_bits_per_sample;

  ppf->info.intensity_target = io.metadata.m.tone_mapping.intensity_target;
  ppf->info.linear_below = io.metadata.m.tone_mapping.linear_below;
  ppf->info.min_nits = io.metadata.m.tone_mapping.min_nits;
  ppf->info.relative_to_max_display =
      TO_JXL_BOOL(io.metadata.m.tone_mapping.relative_to_max_display);

  ppf->info.uses_original_profile = TO_JXL_BOOL(!io.metadata.m.xyb_encoded);
  JXL_ENSURE(0 < io.metadata.m.orientation && io.metadata.m.orientation <= 8);
  ppf->info.orientation =
      static_cast<JxlOrientation>(io.metadata.m.orientation);
  ppf->info.num_color_channels = io.metadata.m.color_encoding.Channels();

  // Convert animation metadata
  JXL_ENSURE(io.frames.size() == 1 || io.metadata.m.have_animation);
  ppf->info.have_animation = TO_JXL_BOOL(io.metadata.m.have_animation);
  ppf->info.animation.tps_numerator = io.metadata.m.animation.tps_numerator;
  ppf->info.animation.tps_denominator = io.metadata.m.animation.tps_denominator;
  ppf->info.animation.num_loops = io.metadata.m.animation.num_loops;

  // Convert the color encoding
  ppf->icc.assign(c_desired.ICC().begin(), c_desired.ICC().end());
  ppf->primary_color_representation =
      c_desired.WantICC() ? PackedPixelFile::kIccIsPrimary
                          : PackedPixelFile::kColorEncodingIsPrimary;
  ppf->color_encoding = c_desired.ToExternal();

  // Convert the extra blobs
  ppf->metadata.exif = io.blobs.exif;
  ppf->metadata.iptc = io.blobs.iptc;
  ppf->metadata.jhgm = io.blobs.jhgm;
  ppf->metadata.jumbf = io.blobs.jumbf;
  ppf->metadata.xmp = io.blobs.xmp;
  const bool float_out = pixel_format.data_type == JXL_TYPE_FLOAT ||
                         pixel_format.data_type == JXL_TYPE_FLOAT16;
  // Convert the pixels
  ppf->frames.clear();
  for (const auto& frame : io.frames) {
    JXL_ENSURE(frame.metadata()->bit_depth.bits_per_sample != 0);
    // It is ok for the frame.color().kNumPlanes to not match the
    // number of channels on the image.
    const uint32_t alpha_channels = has_alpha ? 1 : 0;
    const uint32_t num_channels =
        frame.metadata()->color_encoding.Channels() + alpha_channels;
    JxlPixelFormat format{/*num_channels=*/num_channels,
                          /*data_type=*/pixel_format.data_type,
                          /*endianness=*/pixel_format.endianness,
                          /*align=*/pixel_format.align};

    JXL_ASSIGN_OR_RETURN(PackedFrame packed_frame,
                         PackedFrame::Create(frame.oriented_xsize(),
                                             frame.oriented_ysize(), format));
    JXL_RETURN_IF_ERROR(PackedImage::ValidateDataType(pixel_format.data_type));
    const size_t bits_per_sample =
        float_out ? packed_frame.color.BitsPerChannel(pixel_format.data_type)
                  : ppf->info.bits_per_sample;
    packed_frame.name = frame.name;
    packed_frame.frame_info.name_length = frame.name.size();
    // Color transform
    JXL_ASSIGN_OR_RETURN(ImageBundle ib, frame.Copy());
    const ImageBundle* to_color_transform = &ib;
    ImageMetadata metadata = io.metadata.m;
    ImageBundle store(memory_manager, &metadata);
    const ImageBundle* transformed;
    // TODO(firsching): handle the transform here.
    JXL_RETURN_IF_ERROR(TransformIfNeeded(*to_color_transform, c_desired,
                                          *JxlGetDefaultCms(), pool, &store,
                                          &transformed));

    JXL_RETURN_IF_ERROR(ConvertToExternal(
        *transformed, bits_per_sample, float_out, format.num_channels,
        format.endianness,
        /* stride_out=*/packed_frame.color.stride, pool,
        packed_frame.color.pixels(), packed_frame.color.pixels_size,
        /*out_callback=*/{}, frame.metadata()->GetOrientation()));

    // TODO(firsching): Convert the extra channels, beside one potential alpha
    // channel. FIXME!
    JXL_ENSURE(frame.extra_channels().size() <= (has_alpha ? 1 : 0));
    ppf->frames.push_back(std::move(packed_frame));
  }

  return true;
}
}  // namespace extras
}  // namespace jxl
#endif  // HWY_ONCE
