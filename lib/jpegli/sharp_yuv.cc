// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/jpegli/sharp_yuv.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "lib/jpegli/common.h"
#include "lib/jpegli/encode_internal.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jpegli/sharp_yuv.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace jpegli {
namespace HWY_NAMESPACE {

using hwy::HWY_NAMESPACE::Abs;
using hwy::HWY_NAMESPACE::Add;
using hwy::HWY_NAMESPACE::And;
using hwy::HWY_NAMESPACE::BitCast;
using hwy::HWY_NAMESPACE::ConvertTo;
using hwy::HWY_NAMESPACE::Div;
using hwy::HWY_NAMESPACE::Floor;
using hwy::HWY_NAMESPACE::IfThenElse;
using hwy::HWY_NAMESPACE::Lanes;
using hwy::HWY_NAMESPACE::Le;
using hwy::HWY_NAMESPACE::LoadDup128;
using hwy::HWY_NAMESPACE::LoadInterleaved2;
using hwy::HWY_NAMESPACE::LoadU;
using hwy::HWY_NAMESPACE::Max;
using hwy::HWY_NAMESPACE::Min;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::MulAdd;
using hwy::HWY_NAMESPACE::Or;
using hwy::HWY_NAMESPACE::Rebind;
using hwy::HWY_NAMESPACE::ReduceSum;
using hwy::HWY_NAMESPACE::Set;
using hwy::HWY_NAMESPACE::ShiftLeft;
using hwy::HWY_NAMESPACE::ShiftRight;
using hwy::HWY_NAMESPACE::StoreInterleaved2;
using hwy::HWY_NAMESPACE::StoreU;
using hwy::HWY_NAMESPACE::Sub;
using hwy::HWY_NAMESPACE::Vec;
using hwy::HWY_NAMESPACE::Zero;

using D = HWY_CAPPED(float, 8);
constexpr D d;

// ---------------------------------------------------------------------------
// Scalar helpers (used for tail elements only)
// ---------------------------------------------------------------------------

inline float GammaToLinear(float v) {
  float n = std::max(0.0f, std::min(255.0f, v)) * (1.0f / 255.0f);
  if (n <= 0.04045f) {
    return n / 12.92f;
  } else {
    return std::pow((n + 0.055f) / 1.055f, 2.4f);
  }
}

inline float LinearToGamma(float v) {
  if (v <= 0.0031308f) {
    return v * 12.92f * 255.0f;
  } else {
    return (1.055f * std::pow(std::max(0.0f, v), 1.0f / 2.4f) - 0.055f) *
           255.0f;
  }
}

inline void YCbCrToRGB_Scalar(float y, float cb, float cr, float* r, float* g,
                              float* b) {
  cb -= 128.0f;
  cr -= 128.0f;
  *r = y + cr * 1.402f;
  *g = y + cb * (-0.114f * 1.772f / 0.587f) + cr * (-0.299f * 1.402f / 0.587f);
  *b = y + cb * 1.772f;
}

inline void RGBToYCbCr_Scalar(float r, float g, float b, float* y, float* cb,
                              float* cr) {
  const float kR = 0.299f;
  const float kG = 0.587f;
  const float kB = 0.114f;
  const float kAmpR = 0.701f;
  const float kAmpB = 0.886f;
  const float kDiffR = kAmpR + kR;
  const float kDiffB = kAmpB + kB;
  const float kNormR = 1.0f / (kAmpR + kG + kB);
  const float kNormB = 1.0f / (kR + kG + kAmpB);

  float r_base = r * kR;
  float g_base = g * kG;
  float b_base = b * kB;
  float y_base = r_base + g_base + b_base;
  *y = y_base;
  *cb = (b * kDiffB - y_base) * kNormB + 128.0f;
  *cr = (r * kDiffR - y_base) * kNormR + 128.0f;
}

// ---------------------------------------------------------------------------
// Fast log2/exp2 approximations (same approach as adaptive_quantization.cc)
// ---------------------------------------------------------------------------

// Evaluates a rational polynomial p(x)/q(x) via Horner's scheme.
template <size_t NP, size_t NQ, class DF, class V, typename T>
HWY_INLINE V EvalRationalPoly(const DF df, const V x, const T (&p)[NP],
                              const T (&q)[NQ]) {
  constexpr size_t kDegP = NP / 4 - 1;
  constexpr size_t kDegQ = NQ / 4 - 1;
  auto yp = LoadDup128(df, &p[kDegP * 4]);
  auto yq = LoadDup128(df, &q[kDegQ * 4]);
  HWY_FENCE;
  if (kDegP >= 1) yp = MulAdd(yp, x, LoadDup128(df, p + ((kDegP - 1) * 4)));
  if (kDegQ >= 1) yq = MulAdd(yq, x, LoadDup128(df, q + ((kDegQ - 1) * 4)));
  HWY_FENCE;
  if (kDegP >= 2) yp = MulAdd(yp, x, LoadDup128(df, p + ((kDegP - 2) * 4)));
  if (kDegQ >= 2) yq = MulAdd(yq, x, LoadDup128(df, q + ((kDegQ - 2) * 4)));
  HWY_FENCE;
  if (kDegP >= 3) yp = MulAdd(yp, x, LoadDup128(df, p + ((kDegP - 3) * 4)));
  if (kDegQ >= 3) yq = MulAdd(yq, x, LoadDup128(df, q + ((kDegQ - 3) * 4)));
  return Div(yp, yq);
}

// Fast base-2 logarithm. L1 error ~3.9E-6.
template <class DF, class V>
HWY_INLINE V FastLog2f(const DF df, V x) {
  HWY_ALIGN const float p[4 * 3] = {HWY_REP4(-1.8503833400518310E-06f),
                                    HWY_REP4(1.4287160470083755E+00f),
                                    HWY_REP4(7.4245873327820566E-01f)};
  HWY_ALIGN const float q[4 * 3] = {HWY_REP4(9.9032814277590719E-01f),
                                    HWY_REP4(1.0096718572241148E+00f),
                                    HWY_REP4(1.7409343003366853E-01f)};
  const Rebind<int32_t, DF> di;
  const auto x_bits = BitCast(di, x);
  const auto exp_bits = Sub(x_bits, Set(di, 0x3f2aaaab));
  const auto exp_shifted = ShiftRight<23>(exp_bits);
  const auto mantissa = BitCast(df, Sub(x_bits, ShiftLeft<23>(exp_shifted)));
  const auto exp_val = ConvertTo(df, exp_shifted);
  return Add(EvalRationalPoly(df, Sub(mantissa, Set(df, 1.0f)), p, q), exp_val);
}

// Fast 2^x. Max relative error ~3e-7.
template <class DF, class V>
HWY_INLINE V FastPow2f(const DF df, V x) {
  const Rebind<int32_t, DF> di;
  auto floorx = Floor(x);
  auto exp =
      BitCast(df, ShiftLeft<23>(Add(ConvertTo(di, floorx), Set(di, 127))));
  auto frac = Sub(x, floorx);
  auto num = Add(frac, Set(df, 1.01749063e+01f));
  num = MulAdd(num, frac, Set(df, 4.88687798e+01f));
  num = MulAdd(num, frac, Set(df, 9.85506591e+01f));
  num = Mul(num, exp);
  auto den = MulAdd(frac, Set(df, 2.10242958e-01f), Set(df, -2.22328856e-02f));
  den = MulAdd(den, frac, Set(df, -1.94414990e+01f));
  den = MulAdd(den, frac, Set(df, 9.85506633e+01f));
  return Div(num, den);
}

// Fast pow(x, p) = 2^(p * log2(x)) for x > 0.
template <class DF, class V>
HWY_INLINE V FastPowf(const DF df, V x, float power) {
  return FastPow2f(df, Mul(FastLog2f(df, x), Set(df, power)));
}

// ---------------------------------------------------------------------------
// Highway SIMD gamma helpers using fast polynomial approximations
// ---------------------------------------------------------------------------

template <class D, class V>
HWY_INLINE V GammaToLinear_SIMD(D d, V v) {
  auto v_zero = Set(d, 0.0f);
  auto v_max = Set(d, 255.0f);
  auto n = Mul(Min(Max(v, v_zero), v_max), Set(d, 1.0f / 255.0f));
  auto is_low = Le(n, Set(d, 0.04045f));
  auto low_val = Mul(n, Set(d, 1.0f / 12.92f));
  // High path: ((n + 0.055) / 1.055) ^ 2.4
  auto base = Mul(Add(n, Set(d, 0.055f)), Set(d, 1.0f / 1.055f));
  base = Max(base, Set(d, 1e-6f));
  auto high_val = FastPowf(d, base, 2.4f);
  return IfThenElse(is_low, low_val, high_val);
}

template <class D, class V>
HWY_INLINE V LinearToGamma_SIMD(D d, V v) {
  v = Max(v, Set(d, 0.0f));
  auto is_low = Le(v, Set(d, 0.0031308f));
  auto low_val = Mul(v, Set(d, 12.92f * 255.0f));
  auto v_safe = Max(v, Set(d, 1e-6f));
  auto p = FastPowf(d, v_safe, 1.0f / 2.4f);
  auto high_val =
      Mul(Sub(Mul(p, Set(d, 1.055f)), Set(d, 0.055f)), Set(d, 255.0f));
  return IfThenElse(is_low, low_val, high_val);
}

template <class D, class V>
HWY_INLINE void YCbCrToRGB_SIMD(D d, V y, V cb, V cr, V* r, V* g, V* b) {
  auto v128 = Set(d, 128.0f);
  cb = Sub(cb, v128);
  cr = Sub(cr, v128);
  *r = MulAdd(cr, Set(d, 1.402f), y);
  *g = MulAdd(cb, Set(d, -0.114f * 1.772f / 0.587f),
              MulAdd(cr, Set(d, -0.299f * 1.402f / 0.587f), y));
  *b = MulAdd(cb, Set(d, 1.772f), y);
}

template <class D, class V>
HWY_INLINE V TempLuma_SIMD(D d, V r, V g, V b) {
  // using bt 709 values (0.2126f, 0.7152f, 0.0722f) may be more perceptually
  // accurate, but it needs more visual testing.
  return MulAdd(Set(d, 0.299f), r,
                MulAdd(Set(d, 0.587f), g, Mul(Set(d, 0.114f), b)));
}

// Vectorized RGBToYCbCr: computes Y, Cb, Cr from gamma-space R, G, B vectors.
template <class D, class V>
HWY_INLINE void RGBToYCbCr_SIMD(D d, V r, V g, V b, V* y, V* cb, V* cr) {
  const auto kR = Set(d, 0.299f);
  const auto kG = Set(d, 0.587f);
  const auto kB = Set(d, 0.114f);
  const auto kDiffR = Set(d, 0.701f + 0.299f);
  const auto kDiffB = Set(d, 0.886f + 0.114f);
  const auto kNormR = Set(d, 1.0f / (0.701f + 0.587f + 0.114f));
  const auto kNormB = Set(d, 1.0f / (0.299f + 0.587f + 0.886f));
  const auto v128 = Set(d, 128.0f);

  auto y_base = MulAdd(kR, r, MulAdd(kG, g, Mul(kB, b)));
  *y = y_base;
  // cb = (b * kDiffB - y_base) * kNormB + 128
  *cb = MulAdd(Sub(Mul(b, kDiffB), y_base), kNormB, v128);
  // cr = (r * kDiffR - y_base) * kNormR + 128
  *cr = MulAdd(Sub(Mul(r, kDiffR), y_base), kNormR, v128);
}

// SIMD box-average downsample of linear RGB -> YCbCr chroma
HWY_INLINE void BoxDownsampleRow(const float* lin_r0, const float* lin_g0,
                                 const float* lin_b0, const float* lin_r1,
                                 const float* lin_g1, const float* lin_b1,
                                 size_t down_width, size_t xsize_padded,
                                 float* out_cb, float* out_cr) {
  const size_t N = hwy::HWY_NAMESPACE::Lanes(d);
  const auto quarter = Set(d, 0.25f);
  size_t x = 0;

  for (; x + N <= down_width; x += N) {
    Vec<D> r0e;
    Vec<D> r0o;
    Vec<D> g0e;
    Vec<D> g0o;
    Vec<D> b0e;
    Vec<D> b0o;
    Vec<D> r1e;
    Vec<D> r1o;
    Vec<D> g1e;
    Vec<D> g1o;
    Vec<D> b1e;
    Vec<D> b1o;
    LoadInterleaved2(d, lin_r0 + 2 * x, r0e, r0o);
    LoadInterleaved2(d, lin_g0 + 2 * x, g0e, g0o);
    LoadInterleaved2(d, lin_b0 + 2 * x, b0e, b0o);
    LoadInterleaved2(d, lin_r1 + 2 * x, r1e, r1o);
    LoadInterleaved2(d, lin_g1 + 2 * x, g1e, g1o);
    LoadInterleaved2(d, lin_b1 + 2 * x, b1e, b1o);

    auto sum_r = Add(Add(r0e, r0o), Add(r1e, r1o));
    auto sum_g = Add(Add(g0e, g0o), Add(g1e, g1o));
    auto sum_b = Add(Add(b0e, b0o), Add(b1e, b1o));

    auto avg_r = LinearToGamma_SIMD(d, Mul(sum_r, quarter));
    auto avg_g = LinearToGamma_SIMD(d, Mul(sum_g, quarter));
    auto avg_b = LinearToGamma_SIMD(d, Mul(sum_b, quarter));

    Vec<D> vy;
    Vec<D> vcb;
    Vec<D> vcr;
    RGBToYCbCr_SIMD(d, avg_r, avg_g, avg_b, &vy, &vcb, &vcr);

    StoreU(vcb, d, out_cb + x);
    StoreU(vcr, d, out_cr + x);
  }

  // Scalar tail
  for (; x < down_width; ++x) {
    float lr_sum = 0;
    float lg_sum = 0;
    float lb_sum = 0;
    for (int iy = 0; iy < 2; ++iy) {
      const float* src_r = (iy == 0) ? lin_r0 : lin_r1;
      const float* src_g = (iy == 0) ? lin_g0 : lin_g1;
      const float* src_b = (iy == 0) ? lin_b0 : lin_b1;
      for (int ix = 0; ix < 2; ++ix) {
        size_t si = x * 2 + ix;
        lr_sum += src_r[si];
        lg_sum += src_g[si];
        lb_sum += src_b[si];
      }
    }
    float avg_r = LinearToGamma(lr_sum * 0.25f);
    float avg_g = LinearToGamma(lg_sum * 0.25f);
    float avg_b = LinearToGamma(lb_sum * 0.25f);
    float avg_y;
    float avg_cb;
    float avg_cr;
    RGBToYCbCr_Scalar(avg_r, avg_g, avg_b, &avg_y, &avg_cb, &avg_cr);
    out_cb[x] = avg_cb;
    out_cr[x] = avg_cr;
  }
}

// SIMD tent-filter upsample of one chroma row
HWY_INLINE void TentUpsampleRow(const float* prev_row, const float* cur_row,
                                const float* next_row, size_t down_width,
                                float* out0, float* out1) {
  const size_t N = hwy::HWY_NAMESPACE::Lanes(d);
  const auto k9 = Set(d, 9.0f);
  const auto k3 = Set(d, 3.0f);
  const auto kInv16 = Set(d, 1.0f / 16.0f);
  size_t x = 0;

  for (; x + N <= down_width; x += N) {
    auto vc = LoadU(d, cur_row + x);
    auto vt = LoadU(d, prev_row + x);
    auto vb = LoadU(d, next_row + x);

    auto vl = LoadU(d, cur_row + x - 1);
    auto vtl = LoadU(d, prev_row + x - 1);
    auto vbl = LoadU(d, next_row + x - 1);

    auto vr = LoadU(d, cur_row + x + 1);
    auto vtr = LoadU(d, prev_row + x + 1);
    auto vbr = LoadU(d, next_row + x + 1);

    auto c9 = Mul(vc, k9);

    auto val_tl = Mul(MulAdd(k3, Add(vt, vl), Add(c9, vtl)), kInv16);
    auto val_tr = Mul(MulAdd(k3, Add(vt, vr), Add(c9, vtr)), kInv16);
    auto val_bl = Mul(MulAdd(k3, Add(vb, vl), Add(c9, vbl)), kInv16);
    auto val_br = Mul(MulAdd(k3, Add(vb, vr), Add(c9, vbr)), kInv16);

    StoreInterleaved2(val_tl, val_tr, d, out0 + 2 * x);
    StoreInterleaved2(val_bl, val_br, d, out1 + 2 * x);
  }

  // Scalar tail
  for (; x < down_width; ++x) {
    size_t x_prev = (x == 0) ? 0 : x - 1;
    size_t x_next = (x + 1 == down_width) ? x : x + 1;

    float cc = cur_row[x];
    float ct = prev_row[x];
    float cb = next_row[x];
    float cl = cur_row[x_prev];
    float cr = cur_row[x_next];
    float ctl = prev_row[x_prev];
    float ctr = prev_row[x_next];
    float cbl = next_row[x_prev];
    float cbr = next_row[x_next];

    float c9 = cc * 9.0f;
    float inv16 = 1.0f / 16.0f;
    out0[x * 2 + 0] = (c9 + ct * 3.0f + cl * 3.0f + ctl) * inv16;
    out0[(x * 2 + 1)] = (c9 + ct * 3.0f + cr * 3.0f + ctr) * inv16;
    out1[x * 2 + 0] = (c9 + cb * 3.0f + cl * 3.0f + cbl) * inv16;
    out1[(x * 2 + 1)] = (c9 + cb * 3.0f + cr * 3.0f + cbr) * inv16;
  }
}

void ApplySharpYuvDownsampleImpl(j_compress_ptr cinfo) {
  jpeg_comp_master* m = cinfo->master;
  const size_t iMCU_height = DCTSIZE * cinfo->max_v_samp_factor;
  const size_t y0 = m->next_iMCU_row * iMCU_height;
  const size_t y1 = y0 + iMCU_height;
  const size_t xsize_padded = m->xsize_blocks * DCTSIZE;

  if (cinfo->num_components < 3) return;
  const int h_factor =
      cinfo->max_h_samp_factor / cinfo->comp_info[1].h_samp_factor;
  const int v_factor =
      cinfo->max_v_samp_factor / cinfo->comp_info[1].v_samp_factor;
  if (h_factor != 2 || v_factor != 2) {
    for (int c = 1; c < cinfo->num_components; ++c) {
      jpeg_component_info* comp = &cinfo->comp_info[c];
      const int hf = cinfo->max_h_samp_factor / comp->h_samp_factor;
      const int vf = cinfo->max_v_samp_factor / comp->v_samp_factor;
      if (hf == 1 && vf == 1) continue;
      const size_t y_out0 = y0 / vf;
      float* rows_in[MAX_SAMP_FACTOR];
      auto& input = *m->smooth_input[c];
      auto& output = *m->raw_data[c];
      for (size_t y_in = y0, y_out = y_out0; y_in < y1; y_in += vf, ++y_out) {
        for (int iy = 0; iy < vf; ++iy) {
          rows_in[iy] = input.Row(y_in + iy);
        }
        (*m->downsample_method[c])(rows_in, xsize_padded, output.Row(y_out));
      }
    }
    return;
  }

  const size_t down_width = xsize_padded / 2;
  const size_t down_height = iMCU_height / 2;
  const size_t actual_height =
      (y0 >= cinfo->image_height)
          ? 1
          : std::min(y1, static_cast<size_t>(cinfo->image_height)) - y0;
  const size_t actual_down_height = (actual_height + 1) / 2;
  const size_t high_size = xsize_padded * iMCU_height;

  // Hoisted workspace buffers from master struct.
  jpegli::RowBuffer<float>* ws = m->sharpyuv_workspace;
  auto* target_y_ws = &ws[0];
  auto* best_y_ws = &ws[1];
  auto* lin_r_ws = &ws[2];
  auto* lin_g_ws = &ws[3];
  auto* lin_b_ws = &ws[4];
  auto* up_cb_ws = &ws[5];
  auto* up_cr_ws = &ws[6];
  auto* target_cb_ws = &ws[7];
  auto* target_cr_ws = &ws[8];
  auto* best_cb_ws = &ws[9];
  auto* best_cr_ws = &ws[10];
  auto* err_cb_ws = &ws[11];
  auto* err_cr_ws = &ws[12];

  const size_t N = hwy::HWY_NAMESPACE::Lanes(d);

  const bool have_rgb_input = m->input_rgb[0].HasData();
  // Read pre-transform sRGB directly (if available) or convert from YCbCr.
  for (size_t y = 0; y < iMCU_height; ++y) {
    const float* row_y = m->smooth_input[0]->Row(y0 + y);
    const float* row_cb =
        have_rgb_input ? nullptr : m->smooth_input[1]->Row(y0 + y);
    const float* row_cr =
        have_rgb_input ? nullptr : m->smooth_input[2]->Row(y0 + y);
    const float* row_r = have_rgb_input ? m->input_rgb[0].Row(y0 + y) : nullptr;
    const float* row_g = have_rgb_input ? m->input_rgb[1].Row(y0 + y) : nullptr;
    const float* row_b = have_rgb_input ? m->input_rgb[2].Row(y0 + y) : nullptr;
    float* ty_out = target_y_ws->Row(y);
    float* by_out = best_y_ws->Row(y);
    float* lr_out = lin_r_ws->Row(y);
    float* lg_out = lin_g_ws->Row(y);
    float* lb_out = lin_b_ws->Row(y);

    size_t x = 0;
    for (; x + N <= xsize_padded; x += N) {
      auto py = LoadU(d, row_y + x);
      decltype(py) r;
      decltype(py) g;
      decltype(py) b;
      if (have_rgb_input) {
        r = LoadU(d, row_r + x);
        g = LoadU(d, row_g + x);
        b = LoadU(d, row_b + x);
      } else {
        auto pcb = LoadU(d, row_cb + x);
        auto pcr = LoadU(d, row_cr + x);
        YCbCrToRGB_SIMD(d, py, pcb, pcr, &r, &g, &b);
      }

      auto lin_r = GammaToLinear_SIMD(d, r);
      auto lin_g = GammaToLinear_SIMD(d, g);
      auto lin_b = GammaToLinear_SIMD(d, b);

      auto lin_y = TempLuma_SIMD(d, lin_r, lin_g, lin_b);
      auto t_y = LinearToGamma_SIMD(d, lin_y);

      StoreU(t_y, d, ty_out + x);
      StoreU(py, d, by_out + x);
      StoreU(lin_r, d, lr_out + x);
      StoreU(lin_g, d, lg_out + x);
      StoreU(lin_b, d, lb_out + x);
    }
    for (; x < xsize_padded; ++x) {
      float py = row_y[x];
      float r;
      float g;
      float b;
      if (have_rgb_input) {
        r = row_r[x];
        g = row_g[x];
        b = row_b[x];
      } else {
        float pcb = row_cb[x];
        float pcr = row_cr[x];
        YCbCrToRGB_Scalar(py, pcb, pcr, &r, &g, &b);
      }
      float lr = GammaToLinear(r);
      float lg = GammaToLinear(g);
      float lb = GammaToLinear(b);
      // see TempLuma_SIMD for bt 709 coefficients
      float ly = 0.299f * lr + 0.587f * lg + 0.114f * lb;
      ty_out[x] = LinearToGamma(ly);
      by_out[x] = py;
      lr_out[x] = lr;
      lg_out[x] = lg;
      lb_out[x] = lb;
    }
  }

  // Box-average downsample of linear RGB to target Cb/Cr.
  for (size_t y = 0; y < actual_down_height; ++y) {
    const float* r0 = lin_r_ws->Row(y * 2 + 0);
    const float* g0 = lin_g_ws->Row(y * 2 + 0);
    const float* b0 = lin_b_ws->Row(y * 2 + 0);
    const float* r1 = lin_r_ws->Row(y * 2 + 1);
    const float* g1 = lin_g_ws->Row(y * 2 + 1);
    const float* b1 = lin_b_ws->Row(y * 2 + 1);
    float* dcb = target_cb_ws->Row(y);
    float* dcr = target_cr_ws->Row(y);

    BoxDownsampleRow(r0, g0, b0, r1, g1, b1, down_width, xsize_padded, dcb,
                     dcr);

    // Also initialize best_cb/cr = target_cb/cr
    memcpy(best_cb_ws->Row(y), dcb, down_width * sizeof(float));
    memcpy(best_cr_ws->Row(y), dcr, down_width * sizeof(float));
  }

  for (size_t y = 0; y < actual_down_height; ++y) {
    best_cb_ws->PadRow(y, down_width, 1);
    best_cr_ws->PadRow(y, down_width, 1);
  }

  const auto vmin = Set(d, 0.0f);
  const auto vmax = Set(d, 255.0f);
  float prev_diff_y_sum = 1e30f;
  const float diff_y_threshold = 3.0f * high_size;

  for (int iter = 0; iter < 4; ++iter) {
    float diff_y_sum = 0.0f;
    auto acc_y = Zero(d);

    // Padding for upsampling (all rows for this iteration)
    for (size_t y = 0; y < actual_down_height; ++y) {
      best_cb_ws->PadRow(y, down_width, 1);
      best_cr_ws->PadRow(y, down_width, 1);
    }

    // Pass 1: Upsample current best_cb/cr
    for (size_t y = 0; y < actual_down_height; ++y) {
      size_t y_prev = (y == 0) ? 0 : y - 1;
      size_t y_next = (y + 1 == actual_down_height) ? y : y + 1;
      TentUpsampleRow(best_cb_ws->Row(y_prev), best_cb_ws->Row(y),
                      best_cb_ws->Row(y_next), down_width, up_cb_ws->Row(y * 2),
                      up_cb_ws->Row(y * 2 + 1));
      TentUpsampleRow(best_cr_ws->Row(y_prev), best_cr_ws->Row(y),
                      best_cr_ws->Row(y_next), down_width, up_cr_ws->Row(y * 2),
                      up_cr_ws->Row(y * 2 + 1));
    }

    // Pass 2: Evaluate Y and Prepare for Chroma update
    for (size_t y = 0; y < actual_down_height; ++y) {
      for (int iy = 0; iy < 2; ++iy) {
        size_t yy = y * 2 + iy;
        float* row_best_y = best_y_ws->Row(yy);
        const float* row_up_cb = up_cb_ws->Row(yy);
        const float* row_up_cr = up_cr_ws->Row(yy);
        const float* row_target_y = target_y_ws->Row(yy);
        float* row_lin_r = lin_r_ws->Row(yy);
        float* row_lin_g = lin_g_ws->Row(yy);
        float* row_lin_b = lin_b_ws->Row(yy);

        size_t x = 0;
        for (; x + N <= xsize_padded; x += N) {
          auto py = LoadU(d, row_best_y + x);
          auto ucb = LoadU(d, row_up_cb + x);
          auto ucr = LoadU(d, row_up_cr + x);
          Vec<D> r;
          Vec<D> g;
          Vec<D> b;
          YCbCrToRGB_SIMD(d, py, ucb, ucr, &r, &g, &b);
          auto lin_r = GammaToLinear_SIMD(d, r);
          auto lin_g = GammaToLinear_SIMD(d, g);
          auto lin_b = GammaToLinear_SIMD(d, b);
          auto lin_y = TempLuma_SIMD(d, lin_r, lin_g, lin_b);
          auto dec_y = LinearToGamma_SIMD(d, lin_y);
          auto ey = Sub(LoadU(d, row_target_y + x), dec_y);
          acc_y = Add(acc_y, Abs(ey));

          StoreU(Min(Max(Add(py, ey), vmin), vmax), d, row_best_y + x);
          StoreU(lin_r, d, row_lin_r + x);
          StoreU(lin_g, d, row_lin_g + x);
          StoreU(lin_b, d, row_lin_b + x);
        }
        for (; x < xsize_padded; ++x) {
          float py = row_best_y[x];
          float r;
          float g;
          float b;
          YCbCrToRGB_Scalar(py, row_up_cb[x], row_up_cr[x], &r, &g, &b);
          float lr = GammaToLinear(r);
          float lg = GammaToLinear(g);
          float lb = GammaToLinear(b);
          // see TempLuma_SIMD for bt 709 coefficients
          float ly = 0.299f * lr + 0.587f * lg + 0.114f * lb;
          float ey = row_target_y[x] - LinearToGamma(ly);
          diff_y_sum += std::abs(ey);
          row_best_y[x] = std::max(0.0f, std::min(255.0f, py + ey));
          row_lin_r[x] = lr;
          row_lin_g[x] = lg;
          row_lin_b[x] = lb;
        }
      }

      float* row_best_cb = best_cb_ws->Row(y);
      float* row_best_cr = best_cr_ws->Row(y);
      float* row_err_cb = err_cb_ws->Row(y);
      float* row_err_cr = err_cr_ws->Row(y);
      float* row_tgt_cb = target_cb_ws->Row(y);
      float* row_tgt_cr = target_cr_ws->Row(y);

      BoxDownsampleRow(lin_r_ws->Row(y * 2 + 0), lin_g_ws->Row(y * 2 + 0),
                       lin_b_ws->Row(y * 2 + 0), lin_r_ws->Row(y * 2 + 1),
                       lin_g_ws->Row(y * 2 + 1), lin_b_ws->Row(y * 2 + 1),
                       down_width, xsize_padded, row_err_cb, row_err_cr);

      size_t x = 0;
      for (; x + N <= down_width; x += N) {
        auto bcb = LoadU(d, row_best_cb + x);
        auto bcr = LoadU(d, row_best_cr + x);
        auto tcb = LoadU(d, row_tgt_cb + x);
        auto tcr = LoadU(d, row_tgt_cr + x);
        auto dcb = LoadU(d, row_err_cb + x);
        auto dcr = LoadU(d, row_err_cr + x);
        StoreU(Add(bcb, Sub(tcb, dcb)), d, row_best_cb + x);
        StoreU(Add(bcr, Sub(tcr, dcr)), d, row_best_cr + x);
      }
      for (; x < down_width; ++x) {
        row_best_cb[x] += row_tgt_cb[x] - row_err_cb[x];
        row_best_cr[x] += row_tgt_cr[x] - row_err_cr[x];
      }
    }
    diff_y_sum += ReduceSum(d, acc_y);

    if (iter > 0 &&
        (diff_y_sum < diff_y_threshold || diff_y_sum > prev_diff_y_sum))
      break;
    prev_diff_y_sum = diff_y_sum;
  }

  // Write best_y and best_cb/cr to raw_data output buffers.
  for (size_t y = 0; y < iMCU_height; ++y) {
    float* row_out_y = m->raw_data[0]->Row(y0 + y);
    memcpy(row_out_y, best_y_ws->Row(y), xsize_padded * sizeof(float));
  }

  const size_t y_out0 = y0 / 2;
  for (size_t y_out = 0; y_out < down_height; ++y_out) {
    size_t copy_y = std::min(y_out, actual_down_height - 1);
    float* row_out_cb = m->raw_data[1]->Row(y_out0 + y_out);
    float* row_out_cr = m->raw_data[2]->Row(y_out0 + y_out);
    memcpy(row_out_cb, best_cb_ws->Row(copy_y), down_width * sizeof(float));
    memcpy(row_out_cr, best_cr_ws->Row(copy_y), down_width * sizeof(float));
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jpegli
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jpegli {

HWY_EXPORT(ApplySharpYuvDownsampleImpl);

void ApplySharpYuvDownsample(j_compress_ptr cinfo) {
  HWY_DYNAMIC_DISPATCH(ApplySharpYuvDownsampleImpl)(cinfo);
}

}  // namespace jpegli
#endif  // HWY_ONCE
