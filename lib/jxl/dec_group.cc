// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/jxl/dec_group.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

#include "lib/jxl/frame_header.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/dec_group.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/ac_context.h"
#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/common.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/rect.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/coeff_order.h"
#include "lib/jxl/common.h"  // kMaxNumPasses
#include "lib/jxl/dec_cache.h"
#include "lib/jxl/dec_transforms-inl.h"
#include "lib/jxl/dec_xyb.h"
#include "lib/jxl/entropy_coder.h"
#include "lib/jxl/quant_weights.h"
#include "lib/jxl/quantizer-inl.h"
#include "lib/jxl/quantizer.h"

#ifndef LIB_JXL_DEC_GROUP_CC
#define LIB_JXL_DEC_GROUP_CC
namespace jxl {

struct AuxOut;

// Interface for reading groups for DecodeGroupImpl.
class GetBlock {
 public:
  virtual void StartRow(size_t by) = 0;
  virtual Status LoadBlock(size_t bx, size_t by, const AcStrategy& acs,
                           size_t size, size_t log2_covered_blocks,
                           ACPtr block[3], ACType ac_type) = 0;
  virtual ~GetBlock() {}
};

// Controls whether DecodeGroupImpl renders to pixels or not.
enum DrawMode {
  // Render to pixels.
  kDraw = 0,
  // Don't render to pixels.
  kDontDraw = 1,
};

}  // namespace jxl
#endif  // LIB_JXL_DEC_GROUP_CC

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::AllFalse;
using hwy::HWY_NAMESPACE::Gt;
using hwy::HWY_NAMESPACE::Le;
using hwy::HWY_NAMESPACE::MaskFromVec;
using hwy::HWY_NAMESPACE::Or;
using hwy::HWY_NAMESPACE::Rebind;
using hwy::HWY_NAMESPACE::ShiftRight;

using D = HWY_FULL(float);
using DU = HWY_FULL(uint32_t);
using DI = HWY_FULL(int32_t);
using DI16 = Rebind<int16_t, DI>;
using DI16_FULL = HWY_CAPPED(int16_t, kDCTBlockSize);
constexpr D d;
constexpr DI di;

// TODO(veluca): consider SIMDfying.
void Transpose8x8InPlace(int32_t* JXL_RESTRICT block) {
  for (size_t x = 0; x < 8; x++) {
    for (size_t y = x + 1; y < 8; y++) {
      std::swap(block[y * 8 + x], block[x * 8 + y]);
    }
  }
}

template <ACType ac_type>
void DequantLane(Vec<D> scaled_dequant_x, Vec<D> scaled_dequant_y,
                 Vec<D> scaled_dequant_b,
                 const float* JXL_RESTRICT dequant_matrices, size_t size,
                 size_t k, Vec<D> x_cc_mul, Vec<D> b_cc_mul,
                 const float* JXL_RESTRICT biases, ACPtr qblock[3],
                 float* JXL_RESTRICT block) {
  const auto x_mul = Mul(Load(d, dequant_matrices + k), scaled_dequant_x);
  const auto y_mul =
      Mul(Load(d, dequant_matrices + size + k), scaled_dequant_y);
  const auto b_mul =
      Mul(Load(d, dequant_matrices + 2 * size + k), scaled_dequant_b);

  Vec<DI> quantized_x_int;
  Vec<DI> quantized_y_int;
  Vec<DI> quantized_b_int;
  if (ac_type == ACType::k16) {
    Rebind<int16_t, DI> di16;
    quantized_x_int = PromoteTo(di, Load(di16, qblock[0].ptr16 + k));
    quantized_y_int = PromoteTo(di, Load(di16, qblock[1].ptr16 + k));
    quantized_b_int = PromoteTo(di, Load(di16, qblock[2].ptr16 + k));
  } else {
    quantized_x_int = Load(di, qblock[0].ptr32 + k);
    quantized_y_int = Load(di, qblock[1].ptr32 + k);
    quantized_b_int = Load(di, qblock[2].ptr32 + k);
  }

  const auto dequant_x_cc =
      Mul(AdjustQuantBias(di, 0, quantized_x_int, biases), x_mul);
  const auto dequant_y =
      Mul(AdjustQuantBias(di, 1, quantized_y_int, biases), y_mul);
  const auto dequant_b_cc =
      Mul(AdjustQuantBias(di, 2, quantized_b_int, biases), b_mul);

  const auto dequant_x = MulAdd(x_cc_mul, dequant_y, dequant_x_cc);
  const auto dequant_b = MulAdd(b_cc_mul, dequant_y, dequant_b_cc);
  Store(dequant_x, d, block + k);
  Store(dequant_y, d, block + size + k);
  Store(dequant_b, d, block + 2 * size + k);
}

template <ACType ac_type>
void DequantBlock(const AcStrategy& acs, float inv_global_scale, int quant,
                  float x_dm_multiplier, float b_dm_multiplier, Vec<D> x_cc_mul,
                  Vec<D> b_cc_mul, size_t kind, size_t size,
                  const Quantizer& quantizer, size_t covered_blocks,
                  const size_t* sbx,
                  const float* JXL_RESTRICT* JXL_RESTRICT dc_row,
                  size_t dc_stride, const float* JXL_RESTRICT biases,
                  ACPtr qblock[3], float* JXL_RESTRICT block,
                  float* JXL_RESTRICT scratch) {
  const auto scaled_dequant_s = inv_global_scale / quant;

  const auto scaled_dequant_x = Set(d, scaled_dequant_s * x_dm_multiplier);
  const auto scaled_dequant_y = Set(d, scaled_dequant_s);
  const auto scaled_dequant_b = Set(d, scaled_dequant_s * b_dm_multiplier);

  const float* dequant_matrices = quantizer.DequantMatrix(kind, 0);

  for (size_t k = 0; k < covered_blocks * kDCTBlockSize; k += Lanes(d)) {
    DequantLane<ac_type>(scaled_dequant_x, scaled_dequant_y, scaled_dequant_b,
                         dequant_matrices, size, k, x_cc_mul, b_cc_mul, biases,
                         qblock, block);
  }
  for (size_t c = 0; c < 3; c++) {
    LowestFrequenciesFromDC(acs.Strategy(), dc_row[c] + sbx[c], dc_stride,
                            block + c * size, scratch);
  }
}

Status DecodeGroupImpl(const FrameHeader& frame_header,
                       GetBlock* JXL_RESTRICT get_block,
                       GroupDecCache* JXL_RESTRICT group_dec_cache,
                       PassesDecoderState* JXL_RESTRICT dec_state,
                       size_t thread, size_t group_idx,
                       jpeg::JPEGData* jpeg_data, DrawMode draw) {
  return false;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
namespace {
// Decode quantized AC coefficients of DCT blocks.
// LLF components in the output block will not be modified.
template <ACType ac_type, bool uses_lz77>
Status DecodeACVarBlock(size_t ctx_offset, size_t log2_covered_blocks,
                        int32_t* JXL_RESTRICT row_nzeros,
                        const int32_t* JXL_RESTRICT row_nzeros_top,
                        size_t nzeros_stride, size_t c, size_t bx, size_t by,
                        size_t lbx, AcStrategy acs,
                        const coeff_order_t* JXL_RESTRICT coeff_order,
                        BitReader* JXL_RESTRICT br,
                        ANSSymbolReader* JXL_RESTRICT decoder,
                        const std::vector<uint8_t>& context_map,
                        const uint8_t* qdc_row, const int32_t* qf_row,
                        const BlockCtxMap& block_ctx_map, ACPtr block,
                        size_t shift = 0) {
  // Equal to number of LLF coefficients.
  const size_t covered_blocks = 1 << log2_covered_blocks;
  const size_t size = covered_blocks * kDCTBlockSize;
  int32_t predicted_nzeros =
      PredictFromTopAndLeft(row_nzeros_top, row_nzeros, bx, 32);

  size_t ord = kStrategyOrder[acs.RawStrategy()];
  const coeff_order_t* JXL_RESTRICT order =
      &coeff_order[CoeffOrderOffset(ord, c)];

  size_t block_ctx = block_ctx_map.Context(qdc_row[lbx], qf_row[bx], ord, c);
  const int32_t nzero_ctx =
      block_ctx_map.NonZeroContext(predicted_nzeros, block_ctx) + ctx_offset;

  size_t nzeros =
      decoder->ReadHybridUintInlined<uses_lz77>(nzero_ctx, br, context_map);
  if (nzeros > size - covered_blocks) {
    return JXL_FAILURE("Invalid AC: nzeros %" PRIuS " too large for %" PRIuS
                       " 8x8 blocks",
                       nzeros, covered_blocks);
  }
  for (size_t y = 0; y < acs.covered_blocks_y(); y++) {
    for (size_t x = 0; x < acs.covered_blocks_x(); x++) {
      row_nzeros[bx + x + y * nzeros_stride] =
          (nzeros + covered_blocks - 1) >> log2_covered_blocks;
    }
  }

  const size_t histo_offset =
      ctx_offset + block_ctx_map.ZeroDensityContextsOffset(block_ctx);

  size_t prev = (nzeros > size / 16 ? 0 : 1);
  for (size_t k = covered_blocks; k < size && nzeros != 0; ++k) {
    const size_t ctx =
        histo_offset + ZeroDensityContext(nzeros, k, covered_blocks,
                                          log2_covered_blocks, prev);
    const size_t u_coeff =
        decoder->ReadHybridUintInlined<uses_lz77>(ctx, br, context_map);
    // Hand-rolled version of UnpackSigned, shifting before the conversion to
    // signed integer to avoid undefined behavior of shifting negative numbers.
    const size_t magnitude = u_coeff >> 1;
    const size_t neg_sign = (~u_coeff) & 1;
    const intptr_t coeff =
        static_cast<intptr_t>((magnitude ^ (neg_sign - 1)) << shift);
    if (ac_type == ACType::k16) {
      block.ptr16[order[k]] += coeff;
    } else {
      block.ptr32[order[k]] += coeff;
    }
    prev = static_cast<size_t>(u_coeff != 0);
    nzeros -= prev;
  }
  if (JXL_UNLIKELY(nzeros != 0)) {
    return JXL_FAILURE("Invalid AC: nzeros at end of block is %" PRIuS
                       ", should be 0. Block (%" PRIuS ", %" PRIuS
                       "), channel %" PRIuS,
                       nzeros, bx, by, c);
  }

  return true;
}

// Structs used by DecodeGroupImpl to get a quantized block.
// GetBlockFromBitstream uses ANS decoding (and thus keeps track of row
// pointers in row_nzeros), GetBlockFromEncoder simply reads the coefficient
// image provided by the encoder.

struct GetBlockFromBitstream : public GetBlock {
  void StartRow(size_t by) override {
    qf_row = rect.ConstRow(*qf, by);
    for (size_t c = 0; c < 3; c++) {
      size_t sby = by >> vshift[c];
      quant_dc_row = quant_dc->ConstRow(rect.y0() + by) + rect.x0();
      for (size_t i = 0; i < num_passes; i++) {
        row_nzeros[i][c] = group_dec_cache->num_nzeroes[i].PlaneRow(c, sby);
        row_nzeros_top[i][c] =
            sby == 0
                ? nullptr
                : group_dec_cache->num_nzeroes[i].ConstPlaneRow(c, sby - 1);
      }
    }
  }

  Status LoadBlock(size_t bx, size_t by, const AcStrategy& acs, size_t size,
                   size_t log2_covered_blocks, ACPtr block[3],
                   ACType ac_type) override {
    ;
    for (size_t c : {1, 0, 2}) {
      size_t sbx = bx >> hshift[c];
      size_t sby = by >> vshift[c];
      if (JXL_UNLIKELY((sbx << hshift[c] != bx) || (sby << vshift[c] != by))) {
        continue;
      }

      for (size_t pass = 0; JXL_UNLIKELY(pass < num_passes); pass++) {
        auto decode_ac_varblock =
            decoders[pass].UsesLZ77()
                ? (ac_type == ACType::k16 ? DecodeACVarBlock<ACType::k16, 1>
                                          : DecodeACVarBlock<ACType::k32, 1>)
                : (ac_type == ACType::k16 ? DecodeACVarBlock<ACType::k16, 0>
                                          : DecodeACVarBlock<ACType::k32, 0>);
        JXL_RETURN_IF_ERROR(decode_ac_varblock(
            ctx_offset[pass], log2_covered_blocks, row_nzeros[pass][c],
            row_nzeros_top[pass][c], nzeros_stride, c, sbx, sby, bx, acs,
            &coeff_orders[pass * coeff_order_size], readers[pass],
            &decoders[pass], context_map[pass], quant_dc_row, qf_row,
            *block_ctx_map, block[c], shift_for_pass[pass]));
      }
    }
    return true;
  }

  Status Init(const FrameHeader& frame_header,
              BitReader* JXL_RESTRICT* JXL_RESTRICT readers, size_t num_passes,
              size_t group_idx, size_t histo_selector_bits, const Rect& rect,
              GroupDecCache* JXL_RESTRICT group_dec_cache,
              PassesDecoderState* dec_state, size_t first_pass) {
    for (size_t i = 0; i < 3; i++) {
      hshift[i] = frame_header.chroma_subsampling.HShift(i);
      vshift[i] = frame_header.chroma_subsampling.VShift(i);
    }
    this->coeff_order_size = dec_state->shared->coeff_order_size;
    this->coeff_orders =
        dec_state->shared->coeff_orders.data() + first_pass * coeff_order_size;
    this->context_map = dec_state->context_map.data() + first_pass;
    this->readers = readers;
    this->num_passes = num_passes;
    this->shift_for_pass = frame_header.passes.shift + first_pass;
    this->group_dec_cache = group_dec_cache;
    this->rect = rect;
    block_ctx_map = &dec_state->shared->block_ctx_map;
    qf = &dec_state->shared->raw_quant_field;
    quant_dc = &dec_state->shared->quant_dc;

    for (size_t pass = 0; pass < num_passes; pass++) {
      // Select which histogram set to use among those of the current pass.
      size_t cur_histogram = 0;
      if (histo_selector_bits != 0) {
        cur_histogram = readers[pass]->ReadBits(histo_selector_bits);
      }
      if (cur_histogram >= dec_state->shared->num_histograms) {
        return JXL_FAILURE("Invalid histogram selector");
      }
      ctx_offset[pass] = cur_histogram * block_ctx_map->NumACContexts();

      decoders[pass] =
          ANSSymbolReader(&dec_state->code[pass + first_pass], readers[pass]);
    }
    nzeros_stride = group_dec_cache->num_nzeroes[0].PixelsPerRow();
    for (size_t i = 0; i < num_passes; i++) {
      JXL_ASSERT(
          nzeros_stride ==
          static_cast<size_t>(group_dec_cache->num_nzeroes[i].PixelsPerRow()));
    }
    return true;
  }

  const uint32_t* shift_for_pass = nullptr;  // not owned
  const coeff_order_t* JXL_RESTRICT coeff_orders;
  size_t coeff_order_size;
  const std::vector<uint8_t>* JXL_RESTRICT context_map;
  ANSSymbolReader decoders[kMaxNumPasses];
  BitReader* JXL_RESTRICT* JXL_RESTRICT readers;
  size_t num_passes;
  size_t ctx_offset[kMaxNumPasses];
  size_t nzeros_stride;
  int32_t* JXL_RESTRICT row_nzeros[kMaxNumPasses][3];
  const int32_t* JXL_RESTRICT row_nzeros_top[kMaxNumPasses][3];
  GroupDecCache* JXL_RESTRICT group_dec_cache;
  const BlockCtxMap* block_ctx_map;
  const ImageI* qf;
  const ImageB* quant_dc;
  const int32_t* qf_row;
  const uint8_t* quant_dc_row;
  Rect rect;
  size_t hshift[3], vshift[3];
};

struct GetBlockFromEncoder : public GetBlock {
  void StartRow(size_t by) override {}

  Status LoadBlock(size_t bx, size_t by, const AcStrategy& acs, size_t size,
                   size_t log2_covered_blocks, ACPtr block[3],
                   ACType ac_type) override {
    JXL_DASSERT(ac_type == ACType::k32);
    for (size_t c = 0; c < 3; c++) {
      // for each pass
      for (size_t i = 0; i < quantized_ac->size(); i++) {
        for (size_t k = 0; k < size; k++) {
          // TODO(veluca): SIMD.
          block[c].ptr32[k] +=
              rows[i][c][offset + k] * (1 << shift_for_pass[i]);
        }
      }
    }
    offset += size;
    return true;
  }

  GetBlockFromEncoder(const std::vector<std::unique_ptr<ACImage>>& ac,
                      size_t group_idx, const uint32_t* shift_for_pass)
      : quantized_ac(&ac), shift_for_pass(shift_for_pass) {
    // TODO(veluca): not supported with chroma subsampling.
    for (size_t i = 0; i < quantized_ac->size(); i++) {
      JXL_CHECK((*quantized_ac)[i]->Type() == ACType::k32);
      for (size_t c = 0; c < 3; c++) {
        rows[i][c] = (*quantized_ac)[i]->PlaneRow(c, group_idx, 0).ptr32;
      }
    }
  }

  const std::vector<std::unique_ptr<ACImage>>* JXL_RESTRICT quantized_ac;
  size_t offset = 0;
  const int32_t* JXL_RESTRICT rows[kMaxNumPasses][3];
  const uint32_t* shift_for_pass = nullptr;  // not owned
};

HWY_EXPORT(DecodeGroupImpl);

}  // namespace

Status DecodeGroup(const FrameHeader& frame_header,
                   BitReader* JXL_RESTRICT* JXL_RESTRICT readers,
                   size_t num_passes, size_t group_idx,
                   PassesDecoderState* JXL_RESTRICT dec_state,
                   GroupDecCache* JXL_RESTRICT group_dec_cache, size_t thread,
                   jpeg::JPEGData* JXL_RESTRICT jpeg_data, size_t first_pass,
                   bool force_draw, bool dc_only, bool* should_run_pipeline) {
  return false;
}

Status DecodeGroupForRoundtrip(const FrameHeader& frame_header,
                               const std::vector<std::unique_ptr<ACImage>>& ac,
                               size_t group_idx,
                               PassesDecoderState* JXL_RESTRICT dec_state,
                               GroupDecCache* JXL_RESTRICT group_dec_cache,
                               size_t thread,
                               jpeg::JPEGData* JXL_RESTRICT jpeg_data,
                               AuxOut* aux_out) {
  return false;
}

}  // namespace jxl
#endif  // HWY_ONCE
