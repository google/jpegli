// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/jxl/dec_modular.h"

#include <atomic>
#include <cstdint>
#include <vector>

#include "lib/jxl/frame_header.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/dec_modular.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/rect.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/compressed_dc.h"
#include "lib/jxl/epf.h"
#include "lib/jxl/modular/encoding/encoding.h"
#include "lib/jxl/modular/modular_image.h"
#include "lib/jxl/modular/transform/transform.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Add;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::Rebind;

void MultiplySum(const size_t xsize,
                 const pixel_type* const JXL_RESTRICT row_in,
                 const pixel_type* const JXL_RESTRICT row_in_Y,
                 const float factor, float* const JXL_RESTRICT row_out) {
  const HWY_FULL(float) df;
  const Rebind<pixel_type, HWY_FULL(float)> di;  // assumes pixel_type <= float
  const auto factor_v = Set(df, factor);
  for (size_t x = 0; x < xsize; x += Lanes(di)) {
    const auto in = Add(Load(di, row_in + x), Load(di, row_in_Y + x));
    const auto out = Mul(ConvertTo(df, in), factor_v);
    Store(out, df, row_out + x);
  }
}

void RgbFromSingle(const size_t xsize,
                   const pixel_type* const JXL_RESTRICT row_in,
                   const float factor, float* out_r, float* out_g,
                   float* out_b) {
  const HWY_FULL(float) df;
  const Rebind<pixel_type, HWY_FULL(float)> di;  // assumes pixel_type <= float

  const auto factor_v = Set(df, factor);
  for (size_t x = 0; x < xsize; x += Lanes(di)) {
    const auto in = Load(di, row_in + x);
    const auto out = Mul(ConvertTo(df, in), factor_v);
    Store(out, df, out_r + x);
    Store(out, df, out_g + x);
    Store(out, df, out_b + x);
  }
}

void SingleFromSingle(const size_t xsize,
                      const pixel_type* const JXL_RESTRICT row_in,
                      const float factor, float* row_out) {
  const HWY_FULL(float) df;
  const Rebind<pixel_type, HWY_FULL(float)> di;  // assumes pixel_type <= float

  const auto factor_v = Set(df, factor);
  for (size_t x = 0; x < xsize; x += Lanes(di)) {
    const auto in = Load(di, row_in + x);
    const auto out = Mul(ConvertTo(df, in), factor_v);
    Store(out, df, row_out + x);
  }
}
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
HWY_EXPORT(MultiplySum);       // Local function
HWY_EXPORT(RgbFromSingle);     // Local function
HWY_EXPORT(SingleFromSingle);  // Local function

// Slow conversion using double precision multiplication, only
// needed when the bit depth is too high for single precision
void SingleFromSingleAccurate(const size_t xsize,
                              const pixel_type* const JXL_RESTRICT row_in,
                              const double factor, float* row_out) {
  for (size_t x = 0; x < xsize; x++) {
    row_out[x] = row_in[x] * factor;
  }
}

// convert custom [bits]-bit float (with [exp_bits] exponent bits) stored as int
// back to binary32 float
void int_to_float(const pixel_type* const JXL_RESTRICT row_in,
                  float* const JXL_RESTRICT row_out, const size_t xsize,
                  const int bits, const int exp_bits) {
  if (bits == 32) {
    JXL_ASSERT(sizeof(pixel_type) == sizeof(float));
    JXL_ASSERT(exp_bits == 8);
    memcpy(row_out, row_in, xsize * sizeof(float));
    return;
  }
  int exp_bias = (1 << (exp_bits - 1)) - 1;
  int sign_shift = bits - 1;
  int mant_bits = bits - exp_bits - 1;
  int mant_shift = 23 - mant_bits;
  for (size_t x = 0; x < xsize; ++x) {
    uint32_t f;
    memcpy(&f, &row_in[x], 4);
    int signbit = (f >> sign_shift);
    f &= (1 << sign_shift) - 1;
    if (f == 0) {
      row_out[x] = (signbit ? -0.f : 0.f);
      continue;
    }
    int exp = (f >> mant_bits);
    int mantissa = (f & ((1 << mant_bits) - 1));
    mantissa <<= mant_shift;
    // Try to normalize only if there is space for maneuver.
    if (exp == 0 && exp_bits < 8) {
      // subnormal number
      while ((mantissa & 0x800000) == 0) {
        mantissa <<= 1;
        exp--;
      }
      exp++;
      // remove leading 1 because it is implicit now
      mantissa &= 0x7fffff;
    }
    exp -= exp_bias;
    // broke up the arbitrary float into its parts, now reassemble into
    // binary32
    exp += 127;
    JXL_ASSERT(exp >= 0);
    f = (signbit ? 0x80000000 : 0);
    f |= (exp << 23);
    f |= mantissa;
    memcpy(&row_out[x], &f, 4);
  }
}

#if JXL_DEBUG_V_LEVEL >= 1
std::string ModularStreamId::DebugString() const {
  std::ostringstream os;
  os << (kind == kGlobalData   ? "ModularGlobal"
         : kind == kVarDCTDC   ? "VarDCTDC"
         : kind == kModularDC  ? "ModularDC"
         : kind == kACMetadata ? "ACMeta"
         : kind == kQuantTable ? "QuantTable"
         : kind == kModularAC  ? "ModularAC"
                               : "");
  if (kind == kVarDCTDC || kind == kModularDC || kind == kACMetadata ||
      kind == kModularAC) {
    os << " group " << group_id;
  }
  if (kind == kModularAC) {
    os << " pass " << pass_id;
  }
  if (kind == kQuantTable) {
    os << " " << quant_table_id;
  }
  return os.str();
}
#endif

Status ModularFrameDecoder::DecodeGlobalInfo(BitReader* reader,
                                             const FrameHeader& frame_header,
                                             bool allow_truncated_group) {
  bool decode_color = frame_header.encoding == FrameEncoding::kModular;
  const auto& metadata = frame_header.nonserialized_metadata->m;
  bool is_gray = metadata.color_encoding.IsGray();
  size_t nb_chans = 3;
  if (is_gray && frame_header.color_transform == ColorTransform::kNone) {
    nb_chans = 1;
  }
  do_color = decode_color;
  size_t nb_extra = metadata.extra_channel_info.size();
  bool has_tree = static_cast<bool>(reader->ReadBits(1));
  if (!allow_truncated_group ||
      reader->TotalBitsConsumed() < reader->TotalBytes() * kBitsPerByte) {
    if (has_tree) {
      size_t tree_size_limit =
          std::min(static_cast<size_t>(1 << 22),
                   1024 + frame_dim.xsize * frame_dim.ysize *
                              (nb_chans + nb_extra) / 16);
      JXL_RETURN_IF_ERROR(DecodeTree(reader, &tree, tree_size_limit));
      JXL_RETURN_IF_ERROR(
          DecodeHistograms(reader, (tree.size() + 1) / 2, &code, &context_map));
    }
  }
  if (!do_color) nb_chans = 0;

  bool fp = metadata.bit_depth.floating_point_sample;

  // bits_per_sample is just metadata for XYB images.
  if (metadata.bit_depth.bits_per_sample >= 32 && do_color &&
      frame_header.color_transform != ColorTransform::kXYB) {
    if (metadata.bit_depth.bits_per_sample == 32 && fp == false) {
      return JXL_FAILURE("uint32_t not supported in dec_modular");
    } else if (metadata.bit_depth.bits_per_sample > 32) {
      return JXL_FAILURE("bits_per_sample > 32 not supported");
    }
  }

  JXL_ASSIGN_OR_RETURN(
      Image gi,
      Image::Create(frame_dim.xsize, frame_dim.ysize,
                    metadata.bit_depth.bits_per_sample, nb_chans + nb_extra));

  all_same_shift = true;
  if (frame_header.color_transform == ColorTransform::kYCbCr) {
    for (size_t c = 0; c < nb_chans; c++) {
      gi.channel[c].hshift = frame_header.chroma_subsampling.HShift(c);
      gi.channel[c].vshift = frame_header.chroma_subsampling.VShift(c);
      size_t xsize_shifted =
          DivCeil(frame_dim.xsize, 1 << gi.channel[c].hshift);
      size_t ysize_shifted =
          DivCeil(frame_dim.ysize, 1 << gi.channel[c].vshift);
      JXL_RETURN_IF_ERROR(gi.channel[c].shrink(xsize_shifted, ysize_shifted));
      if (gi.channel[c].hshift != gi.channel[0].hshift ||
          gi.channel[c].vshift != gi.channel[0].vshift)
        all_same_shift = false;
    }
  }

  for (size_t ec = 0, c = nb_chans; ec < nb_extra; ec++, c++) {
    size_t ecups = frame_header.extra_channel_upsampling[ec];
    JXL_RETURN_IF_ERROR(
        gi.channel[c].shrink(DivCeil(frame_dim.xsize_upsampled, ecups),
                             DivCeil(frame_dim.ysize_upsampled, ecups)));
    gi.channel[c].hshift = gi.channel[c].vshift =
        CeilLog2Nonzero(ecups) - CeilLog2Nonzero(frame_header.upsampling);
    if (gi.channel[c].hshift != gi.channel[0].hshift ||
        gi.channel[c].vshift != gi.channel[0].vshift)
      all_same_shift = false;
  }

  JXL_DEBUG_V(6, "DecodeGlobalInfo: full_image (w/o transforms) %s",
              gi.DebugString().c_str());
  ModularOptions options;
  options.max_chan_size = frame_dim.group_dim;
  options.group_dim = frame_dim.group_dim;
  Status dec_status = ModularGenericDecompress(
      reader, gi, &global_header, ModularStreamId::Global().ID(frame_dim),
      &options,
      /*undo_transforms=*/false, &tree, &code, &context_map,
      allow_truncated_group);
  if (!allow_truncated_group) JXL_RETURN_IF_ERROR(dec_status);
  if (dec_status.IsFatalError()) {
    return JXL_FAILURE("Failed to decode global modular info");
  }

  // TODO(eustas): are we sure this can be done after partial decode?
  have_something = false;
  for (size_t c = 0; c < gi.channel.size(); c++) {
    Channel& gic = gi.channel[c];
    if (c >= gi.nb_meta_channels && gic.w <= frame_dim.group_dim &&
        gic.h <= frame_dim.group_dim)
      have_something = true;
  }
  // move global transforms to groups if possible
  if (!have_something && all_same_shift) {
    if (gi.transform.size() == 1 && gi.transform[0].id == TransformId::kRCT) {
      global_transform = gi.transform;
      gi.transform.clear();
      // TODO(jon): also move no-delta-palette out (trickier though)
    }
  }
  full_image = std::move(gi);
  JXL_DEBUG_V(6, "DecodeGlobalInfo: full_image (with transforms) %s",
              full_image.DebugString().c_str());
  return dec_status;
}

void ModularFrameDecoder::MaybeDropFullImage() {
  if (full_image.transform.empty() && !have_something && all_same_shift) {
    use_full_image = false;
    JXL_DEBUG_V(6, "Dropping full image");
    for (auto& ch : full_image.channel) {
      // keep metadata on channels around, but dealloc their planes
      ch.plane = Plane<pixel_type>();
    }
  }
}

Status ModularFrameDecoder::DecodeGroup(
    const FrameHeader& frame_header, const Rect& rect, BitReader* reader,
    int minShift, int maxShift, const ModularStreamId& stream, bool zerofill,
    PassesDecoderState* dec_state,
    bool allow_truncated, bool* should_run_pipeline) {
  return false;
}

Status ModularFrameDecoder::DecodeVarDCTDC(const FrameHeader& frame_header,
                                           size_t group_id, BitReader* reader,
                                           PassesDecoderState* dec_state) {
  const Rect r = dec_state->shared->frame_dim.DCGroupRect(group_id);
  JXL_DEBUG_V(6, "Decoding VarDCT DC with rect %s", Description(r).c_str());
  // TODO(eustas): investigate if we could reduce the impact of
  //               EvalRationalPolynomial; generally speaking, the limit is
  //               2**(128/(3*magic)), where 128 comes from IEEE 754 exponent,
  //               3 comes from XybToRgb that cubes the values, and "magic" is
  //               the sum of all other contributions. 2**18 is known to lead
  //               to NaN on input found by fuzzing (see commit message).
  JXL_ASSIGN_OR_RETURN(
      Image image, Image::Create(r.xsize(), r.ysize(), full_image.bitdepth, 3));
  size_t stream_id = ModularStreamId::VarDCTDC(group_id).ID(frame_dim);
  reader->Refill();
  size_t extra_precision = reader->ReadFixedBits<2>();
  float mul = 1.0f / (1 << extra_precision);
  ModularOptions options;
  for (size_t c = 0; c < 3; c++) {
    Channel& ch = image.channel[c < 2 ? c ^ 1 : c];
    ch.w >>= frame_header.chroma_subsampling.HShift(c);
    ch.h >>= frame_header.chroma_subsampling.VShift(c);
    JXL_RETURN_IF_ERROR(ch.shrink());
  }
  if (!ModularGenericDecompress(
          reader, image, /*header=*/nullptr, stream_id, &options,
          /*undo_transforms=*/true, &tree, &code, &context_map)) {
    return JXL_FAILURE("Failed to decode VarDCT DC group (DC group id %d)",
                       static_cast<int>(group_id));
  }
  DequantDC(r, &dec_state->shared_storage.dc_storage,
            &dec_state->shared_storage.quant_dc, image,
            dec_state->shared->quantizer.MulDC(), mul,
            dec_state->shared->cmap.DCFactors(),
            frame_header.chroma_subsampling, dec_state->shared->block_ctx_map);
  return true;
}

Status ModularFrameDecoder::DecodeAcMetadata(const FrameHeader& frame_header,
                                             size_t group_id, BitReader* reader,
                                             PassesDecoderState* dec_state) {
  const Rect r = dec_state->shared->frame_dim.DCGroupRect(group_id);
  JXL_DEBUG_V(6, "Decoding AcMetadata with rect %s", Description(r).c_str());
  size_t upper_bound = r.xsize() * r.ysize();
  reader->Refill();
  size_t count = reader->ReadBits(CeilLog2Nonzero(upper_bound)) + 1;
  size_t stream_id = ModularStreamId::ACMetadata(group_id).ID(frame_dim);
  // YToX, YToB, ACS + QF, EPF
  JXL_ASSIGN_OR_RETURN(
      Image image, Image::Create(r.xsize(), r.ysize(), full_image.bitdepth, 4));
  static_assert(kColorTileDimInBlocks == 8, "Color tile size changed");
  Rect cr(r.x0() >> 3, r.y0() >> 3, (r.xsize() + 7) >> 3, (r.ysize() + 7) >> 3);
  JXL_ASSIGN_OR_RETURN(image.channel[0],
                       Channel::Create(cr.xsize(), cr.ysize(), 3, 3));
  JXL_ASSIGN_OR_RETURN(image.channel[1],
                       Channel::Create(cr.xsize(), cr.ysize(), 3, 3));
  JXL_ASSIGN_OR_RETURN(image.channel[2], Channel::Create(count, 2, 0, 0));
  ModularOptions options;
  if (!ModularGenericDecompress(
          reader, image, /*header=*/nullptr, stream_id, &options,
          /*undo_transforms=*/true, &tree, &code, &context_map)) {
    return JXL_FAILURE("Failed to decode AC metadata");
  }
  ConvertPlaneAndClamp(Rect(image.channel[0].plane), image.channel[0].plane, cr,
                       &dec_state->shared_storage.cmap.ytox_map);
  ConvertPlaneAndClamp(Rect(image.channel[1].plane), image.channel[1].plane, cr,
                       &dec_state->shared_storage.cmap.ytob_map);
  size_t num = 0;
  bool is444 = frame_header.chroma_subsampling.Is444();
  auto& ac_strategy = dec_state->shared_storage.ac_strategy;
  size_t xlim = std::min(ac_strategy.xsize(), r.x0() + r.xsize());
  size_t ylim = std::min(ac_strategy.ysize(), r.y0() + r.ysize());
  uint32_t local_used_acs = 0;
  for (size_t iy = 0; iy < r.ysize(); iy++) {
    size_t y = r.y0() + iy;
    int32_t* row_qf = r.Row(&dec_state->shared_storage.raw_quant_field, iy);
    uint8_t* row_epf = r.Row(&dec_state->shared_storage.epf_sharpness, iy);
    int32_t* row_in_1 = image.channel[2].plane.Row(0);
    int32_t* row_in_2 = image.channel[2].plane.Row(1);
    int32_t* row_in_3 = image.channel[3].plane.Row(iy);
    for (size_t ix = 0; ix < r.xsize(); ix++) {
      size_t x = r.x0() + ix;
      int sharpness = row_in_3[ix];
      if (sharpness < 0 || sharpness >= LoopFilter::kEpfSharpEntries) {
        return JXL_FAILURE("Corrupted sharpness field");
      }
      row_epf[ix] = sharpness;
      if (ac_strategy.IsValid(x, y)) {
        continue;
      }

      if (num >= count) return JXL_FAILURE("Corrupted stream");

      if (!AcStrategy::IsRawStrategyValid(row_in_1[num])) {
        return JXL_FAILURE("Invalid AC strategy");
      }
      local_used_acs |= 1u << row_in_1[num];
      AcStrategy acs = AcStrategy::FromRawStrategy(row_in_1[num]);
      if ((acs.covered_blocks_x() > 1 || acs.covered_blocks_y() > 1) &&
          !is444) {
        return JXL_FAILURE(
            "AC strategy not compatible with chroma subsampling");
      }
      // Ensure that blocks do not overflow *AC* groups.
      size_t next_x_ac_block = (x / kGroupDimInBlocks + 1) * kGroupDimInBlocks;
      size_t next_y_ac_block = (y / kGroupDimInBlocks + 1) * kGroupDimInBlocks;
      size_t next_x_dct_block = x + acs.covered_blocks_x();
      size_t next_y_dct_block = y + acs.covered_blocks_y();
      if (next_x_dct_block > next_x_ac_block || next_x_dct_block > xlim) {
        return JXL_FAILURE("Invalid AC strategy, x overflow");
      }
      if (next_y_dct_block > next_y_ac_block || next_y_dct_block > ylim) {
        return JXL_FAILURE("Invalid AC strategy, y overflow");
      }
      JXL_RETURN_IF_ERROR(
          ac_strategy.SetNoBoundsCheck(x, y, AcStrategy::Type(row_in_1[num])));
      row_qf[ix] = 1 + std::max<int32_t>(0, std::min(Quantizer::kQuantMax - 1,
                                                     row_in_2[num]));
      num++;
    }
  }
  dec_state->used_acs |= local_used_acs;
  if (frame_header.loop_filter.epf_iters > 0) {
    ComputeSigma(frame_header.loop_filter, r, dec_state);
  }
  return true;
}

Status ModularFrameDecoder::ModularImageToDecodedRect(
    const FrameHeader& frame_header, Image& gi, PassesDecoderState* dec_state,
    jxl::ThreadPool* pool,
    Rect modular_rect) const {
  return false;
}

Status ModularFrameDecoder::FinalizeDecoding(const FrameHeader& frame_header,
                                             PassesDecoderState* dec_state,
                                             jxl::ThreadPool* pool,
                                             bool inplace) {
  return false;
}

static constexpr const float kAlmostZero = 1e-8f;

Status ModularFrameDecoder::DecodeQuantTable(
    size_t required_size_x, size_t required_size_y, BitReader* br,
    QuantEncoding* encoding, size_t idx,
    ModularFrameDecoder* modular_frame_decoder) {
  JXL_RETURN_IF_ERROR(F16Coder::Read(br, &encoding->qraw.qtable_den));
  if (encoding->qraw.qtable_den < kAlmostZero) {
    // qtable[] values are already checked for <= 0 so the denominator may not
    // be negative.
    return JXL_FAILURE("Invalid qtable_den: value too small");
  }
  JXL_ASSIGN_OR_RETURN(Image image,
                       Image::Create(required_size_x, required_size_y, 8, 3));
  ModularOptions options;
  if (modular_frame_decoder) {
    JXL_RETURN_IF_ERROR(ModularGenericDecompress(
        br, image, /*header=*/nullptr,
        ModularStreamId::QuantTable(idx).ID(modular_frame_decoder->frame_dim),
        &options, /*undo_transforms=*/true, &modular_frame_decoder->tree,
        &modular_frame_decoder->code, &modular_frame_decoder->context_map));
  } else {
    JXL_RETURN_IF_ERROR(ModularGenericDecompress(br, image, /*header=*/nullptr,
                                                 0, &options,
                                                 /*undo_transforms=*/true));
  }
  if (!encoding->qraw.qtable) {
    encoding->qraw.qtable = new std::vector<int>();
  }
  encoding->qraw.qtable->resize(required_size_x * required_size_y * 3);
  for (size_t c = 0; c < 3; c++) {
    for (size_t y = 0; y < required_size_y; y++) {
      int32_t* JXL_RESTRICT row = image.channel[c].Row(y);
      for (size_t x = 0; x < required_size_x; x++) {
        (*encoding->qraw.qtable)[c * required_size_x * required_size_y +
                                 y * required_size_x + x] = row[x];
        if (row[x] <= 0) {
          return JXL_FAILURE("Invalid raw quantization table");
        }
      }
    }
  }
  return true;
}

}  // namespace jxl
#endif  // HWY_ONCE
