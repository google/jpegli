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
#include <vector>

#include "lib/jpegli/common.h"
#include "lib/jpegli/encode_internal.h"
#include "lib/jpegli/simd.h"

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
using hwy::HWY_NAMESPACE::Le;
using hwy::HWY_NAMESPACE::LoadDup128;
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
using hwy::HWY_NAMESPACE::StoreU;
using hwy::HWY_NAMESPACE::Sub;
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
  *g = y + cb * (-0.114f * 1.772f / 0.587f) +
       cr * (-0.299f * 1.402f / 0.587f);
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

// ---------------------------------------------------------------------------
// SIMD box-average downsample of linear RGB -> YCbCr chroma
// Processes each row of down_width pixels. For each output pixel, averages
// a 2x2 block of linear RGB, converts back to gamma, then to YCbCr.
// ---------------------------------------------------------------------------
HWY_INLINE void BoxAverageDownsampleRow(const float* lin_r0,
                                        const float* lin_g0,
                                        const float* lin_b0,
                                        const float* lin_r1,
                                        const float* lin_g1,
                                        const float* lin_b1,
                                        size_t down_width, size_t xsize_padded,
                                        float* out_cb, float* out_cr) {
  const size_t N = hwy::HWY_NAMESPACE::Lanes(d);
  const auto quarter = Set(d, 0.25f);
  size_t x = 0;

  // SIMD path: process N output pixels at a time.
  // Each output pixel needs 2 input pixels horizontally, so we need 2*N inputs.
  for (; x + N <= down_width; x += N) {
    // Gather even and odd columns from two rows.
    // Row 0: indices 2*x, 2*x+1, 2*x+2, 2*x+3, ...
    // Row 1: same pattern.
    // We accumulate the 4 samples for each output pixel.
    auto sum_r = Zero(d);
    auto sum_g = Zero(d);
    auto sum_b = Zero(d);

    for (int iy = 0; iy < 2; ++iy) {
      const float* src_r = (iy == 0) ? lin_r0 : lin_r1;
      const float* src_g = (iy == 0) ? lin_g0 : lin_g1;
      const float* src_b = (iy == 0) ? lin_b0 : lin_b1;
      for (int ix = 0; ix < 2; ++ix) {
        // Load non-contiguous: elements at positions 2*x+ix, 2*(x+1)+ix, ...
        // Unfortunately these are strided, so we load pairs and deinterleave
        // is cleaner. For simplicity and correctness, use scalar gather here
        // since the inner 2x2 loop is just 4 iterations.
        // Actually, let's use a temporary buffer approach for the gather.
        HWY_ALIGN float tmp_r[HWY_MAX_LANES_D(D)];
        HWY_ALIGN float tmp_g[HWY_MAX_LANES_D(D)];
        HWY_ALIGN float tmp_b[HWY_MAX_LANES_D(D)];
        for (size_t k = 0; k < N; ++k) {
          size_t si = (x + k) * 2 + ix;
          tmp_r[k] = src_r[si];
          tmp_g[k] = src_g[si];
          tmp_b[k] = src_b[si];
        }
        sum_r = Add(sum_r, LoadU(d, tmp_r));
        sum_g = Add(sum_g, LoadU(d, tmp_g));
        sum_b = Add(sum_b, LoadU(d, tmp_b));
      }
    }

    auto avg_r = LinearToGamma_SIMD(d, Mul(sum_r, quarter));
    auto avg_g = LinearToGamma_SIMD(d, Mul(sum_g, quarter));
    auto avg_b = LinearToGamma_SIMD(d, Mul(sum_b, quarter));

    decltype(avg_r) vy, vcb, vcr;
    RGBToYCbCr_SIMD(d, avg_r, avg_g, avg_b, &vy, &vcb, &vcr);

    StoreU(vcb, d, out_cb + x);
    StoreU(vcr, d, out_cr + x);
  }

  // Scalar tail
  for (; x < down_width; ++x) {
    float lr_sum = 0, lg_sum = 0, lb_sum = 0;
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
    float avg_y, avg_cb, avg_cr;
    RGBToYCbCr_Scalar(avg_r, avg_g, avg_b, &avg_y, &avg_cb, &avg_cr);
    out_cb[x] = avg_cb;
    out_cr[x] = avg_cr;
  }
}

// ---------------------------------------------------------------------------
// SIMD tent-filter upsample of one chroma row
// For each x in [0, down_width), computes 4 upsampled values using the
// 9/3/3/1 tent filter, and writes them to the 4 corresponding full-res
// positions.
// ---------------------------------------------------------------------------
HWY_INLINE void TentUpsampleRow(const float* prev_row, const float* cur_row,
                                const float* next_row, size_t down_width,
                                size_t xsize_padded, size_t y,
                                float* out_full) {
  const size_t N = hwy::HWY_NAMESPACE::Lanes(d);
  const auto k9 = Set(d, 9.0f);
  const auto k3 = Set(d, 3.0f);
  const auto kInv16 = Set(d, 1.0f / 16.0f);
  size_t x = 0;

  for (; x + N <= down_width; x += N) {
    auto vc = LoadU(d, cur_row + x);

    // Load left neighbors (clamp at boundary)
    HWY_ALIGN float tmp_l[HWY_MAX_LANES_D(D)];
    HWY_ALIGN float tmp_tl[HWY_MAX_LANES_D(D)];
    HWY_ALIGN float tmp_bl[HWY_MAX_LANES_D(D)];
    for (size_t k = 0; k < N; ++k) {
      size_t xi = x + k;
      size_t xl = (xi == 0) ? 0 : xi - 1;
      tmp_l[k] = cur_row[xl];
      tmp_tl[k] = prev_row[xl];
      tmp_bl[k] = next_row[xl];
    }
    auto vl = LoadU(d, tmp_l);
    auto vtl = LoadU(d, tmp_tl);
    auto vbl = LoadU(d, tmp_bl);

    // Load right neighbors (clamp at boundary)
    HWY_ALIGN float tmp_r[HWY_MAX_LANES_D(D)];
    HWY_ALIGN float tmp_tr[HWY_MAX_LANES_D(D)];
    HWY_ALIGN float tmp_br[HWY_MAX_LANES_D(D)];
    for (size_t k = 0; k < N; ++k) {
      size_t xi = x + k;
      size_t xr = (xi + 1 == down_width) ? xi : xi + 1;
      tmp_r[k] = cur_row[xr];
      tmp_tr[k] = prev_row[xr];
      tmp_br[k] = next_row[xr];
    }
    auto vr = LoadU(d, tmp_r);
    auto vtr = LoadU(d, tmp_tr);
    auto vbr = LoadU(d, tmp_br);

    auto vt = LoadU(d, prev_row + x);
    auto vb = LoadU(d, next_row + x);

    // c9 = c * 9
    auto c9 = Mul(vc, k9);

    // Top-left quadrant: (c*9 + t*3 + l*3 + tl) / 16
    auto val_tl = Mul(MulAdd(k3, Add(vt, vl), Add(c9, vtl)), kInv16);
    // Top-right quadrant: (c*9 + t*3 + r*3 + tr) / 16
    auto val_tr = Mul(MulAdd(k3, Add(vt, vr), Add(c9, vtr)), kInv16);
    // Bottom-left quadrant: (c*9 + b*3 + l*3 + bl) / 16
    auto val_bl = Mul(MulAdd(k3, Add(vb, vl), Add(c9, vbl)), kInv16);
    // Bottom-right quadrant: (c*9 + b*3 + r*3 + br) / 16
    auto val_br = Mul(MulAdd(k3, Add(vb, vr), Add(c9, vbr)), kInv16);

    // Scatter to full-res positions
    HWY_ALIGN float res_tl[HWY_MAX_LANES_D(D)];
    HWY_ALIGN float res_tr[HWY_MAX_LANES_D(D)];
    HWY_ALIGN float res_bl[HWY_MAX_LANES_D(D)];
    HWY_ALIGN float res_br[HWY_MAX_LANES_D(D)];
    StoreU(val_tl, d, res_tl);
    StoreU(val_tr, d, res_tr);
    StoreU(val_bl, d, res_bl);
    StoreU(val_br, d, res_br);
    for (size_t k = 0; k < N; ++k) {
      size_t xi = x + k;
      out_full[(y * 2 + 0) * xsize_padded + (xi * 2 + 0)] = res_tl[k];
      out_full[(y * 2 + 0) * xsize_padded + (xi * 2 + 1)] = res_tr[k];
      out_full[(y * 2 + 1) * xsize_padded + (xi * 2 + 0)] = res_bl[k];
      out_full[(y * 2 + 1) * xsize_padded + (xi * 2 + 1)] = res_br[k];
    }
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
    out_full[(y * 2 + 0) * xsize_padded + (x * 2 + 0)] =
        (c9 + ct * 3.0f + cl * 3.0f + ctl) * inv16;
    out_full[(y * 2 + 0) * xsize_padded + (x * 2 + 1)] =
        (c9 + ct * 3.0f + cr * 3.0f + ctr) * inv16;
    out_full[(y * 2 + 1) * xsize_padded + (x * 2 + 0)] =
        (c9 + cb * 3.0f + cl * 3.0f + cbl) * inv16;
    out_full[(y * 2 + 1) * xsize_padded + (x * 2 + 1)] =
        (c9 + cb * 3.0f + cr * 3.0f + cbr) * inv16;
  }
}

// ---------------------------------------------------------------------------
// Main SharpYUV implementation
// ---------------------------------------------------------------------------

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
      for (size_t y_in = y0, y_out = y_out0; y_in < y1;
           y_in += vf, ++y_out) {
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
  const size_t high_size = xsize_padded * iMCU_height;
  const size_t low_size = down_width * down_height;

  std::vector<float> target_y(high_size);
  std::vector<float> best_y(high_size);
  std::vector<float> tmp_lin_r(high_size);
  std::vector<float> tmp_lin_g(high_size);
  std::vector<float> tmp_lin_b(high_size);

  std::vector<float> target_cb(low_size);
  std::vector<float> target_cr(low_size);
  std::vector<float> best_cb(low_size);
  std::vector<float> best_cr(low_size);

  std::vector<float> up_cb(high_size);
  std::vector<float> up_cr(high_size);

  const size_t N = hwy::HWY_NAMESPACE::Lanes(d);

  // 1. Decode input YCbCr -> linear RGB, compute per-pixel target luma (SIMD)
  for (size_t y = 0; y < iMCU_height; ++y) {
    const float* row_y = m->smooth_input[0]->Row(y0 + y);
    const float* row_cb = m->smooth_input[1]->Row(y0 + y);
    const float* row_cr = m->smooth_input[2]->Row(y0 + y);
    size_t x = 0;
    for (; x + N <= xsize_padded; x += N) {
      auto py = LoadU(d, row_y + x);
      auto pcb = LoadU(d, row_cb + x);
      auto pcr = LoadU(d, row_cr + x);

      decltype(py) r, g, b;
      YCbCrToRGB_SIMD(d, py, pcb, pcr, &r, &g, &b);

      auto lin_r = GammaToLinear_SIMD(d, r);
      auto lin_g = GammaToLinear_SIMD(d, g);
      auto lin_b = GammaToLinear_SIMD(d, b);

      auto lin_y = TempLuma_SIMD(d, lin_r, lin_g, lin_b);
      auto t_y = LinearToGamma_SIMD(d, lin_y);

      size_t idx = y * xsize_padded + x;
      StoreU(t_y, d, &target_y[idx]);
      StoreU(py, d, &best_y[idx]);
      StoreU(lin_r, d, &tmp_lin_r[idx]);
      StoreU(lin_g, d, &tmp_lin_g[idx]);
      StoreU(lin_b, d, &tmp_lin_b[idx]);
    }
    for (; x < xsize_padded; ++x) {
      float py = row_y[x];
      float pcb = row_cb[x];
      float pcr = row_cr[x];
      float r, g, b;
      YCbCrToRGB_Scalar(py, pcb, pcr, &r, &g, &b);
      float lr = GammaToLinear(r);
      float lg = GammaToLinear(g);
      float lb = GammaToLinear(b);
      float ly = 0.299f * lr + 0.587f * lg + 0.114f * lb;
      size_t idx = y * xsize_padded + x;
      target_y[idx] = LinearToGamma(ly);
      best_y[idx] = py;
      tmp_lin_r[idx] = lr;
      tmp_lin_g[idx] = lg;
      tmp_lin_b[idx] = lb;
    }
  }

  // 2. Box-average downsample of linear RGB -> target Cb/Cr (SIMD)
  for (size_t y = 0; y < down_height; ++y) {
    const float* r0 = &tmp_lin_r[(y * 2 + 0) * xsize_padded];
    const float* g0 = &tmp_lin_g[(y * 2 + 0) * xsize_padded];
    const float* b0 = &tmp_lin_b[(y * 2 + 0) * xsize_padded];
    const float* r1 = &tmp_lin_r[(y * 2 + 1) * xsize_padded];
    const float* g1 = &tmp_lin_g[(y * 2 + 1) * xsize_padded];
    const float* b1 = &tmp_lin_b[(y * 2 + 1) * xsize_padded];
    float* dcb = &target_cb[y * down_width];
    float* dcr = &target_cr[y * down_width];

    BoxAverageDownsampleRow(r0, g0, b0, r1, g1, b1, down_width, xsize_padded,
                            dcb, dcr);

    // Also initialize best_cb/cr = target_cb/cr
    memcpy(&best_cb[y * down_width], dcb, down_width * sizeof(float));
    memcpy(&best_cr[y * down_width], dcr, down_width * sizeof(float));
  }

  float prev_diff_y_sum = 1e30f;
  const float diff_y_threshold = 3.0f * high_size;

  // Pre-allocate error buffers outside the iteration loop to avoid repeated
  // heap allocation/deallocation.
  std::vector<float> err_y(high_size);
  std::vector<float> err_cb(low_size);
  std::vector<float> err_cr(low_size);

  for (int iter = 0; iter < 4; ++iter) {
    memset(err_y.data(), 0, high_size * sizeof(float));
    memset(err_cb.data(), 0, low_size * sizeof(float));
    memset(err_cr.data(), 0, low_size * sizeof(float));
    float diff_y_sum = 0.0f;

    // 3a. Upsample best_cb/cr to full-res using a 2D tent (9/3/3/1) filter
    //     (SIMD inner loop with scalar boundary handling)
    for (size_t y = 0; y < down_height; ++y) {
      size_t y_prev = (y == 0) ? 0 : y - 1;
      size_t y_next = (y + 1 == down_height) ? y : y + 1;

      const float* cb_prev = &best_cb[y_prev * down_width];
      const float* cb_cur = &best_cb[y * down_width];
      const float* cb_next = &best_cb[y_next * down_width];
      TentUpsampleRow(cb_prev, cb_cur, cb_next, down_width, xsize_padded, y,
                      up_cb.data());

      const float* cr_prev = &best_cr[y_prev * down_width];
      const float* cr_cur = &best_cr[y * down_width];
      const float* cr_next = &best_cr[y_next * down_width];
      TentUpsampleRow(cr_prev, cr_cur, cr_next, down_width, xsize_padded, y,
                      up_cr.data());
    }

    // 3b. Evaluate YCbCr reconstruction error at full resolution (SIMD)
    for (size_t y = 0; y < iMCU_height; ++y) {
      size_t x = 0;
      for (; x + N <= xsize_padded; x += N) {
        size_t idx = y * xsize_padded + x;
        auto py = LoadU(d, &best_y[idx]);
        auto ucb = LoadU(d, &up_cb[idx]);
        auto ucr = LoadU(d, &up_cr[idx]);

        decltype(py) r, g, b;
        YCbCrToRGB_SIMD(d, py, ucb, ucr, &r, &g, &b);

        auto lin_r = GammaToLinear_SIMD(d, r);
        auto lin_g = GammaToLinear_SIMD(d, g);
        auto lin_b = GammaToLinear_SIMD(d, b);

        auto lin_y = TempLuma_SIMD(d, lin_r, lin_g, lin_b);
        auto dec_y = LinearToGamma_SIMD(d, lin_y);

        auto e_y = Sub(LoadU(d, &target_y[idx]), dec_y);
        StoreU(e_y, d, &err_y[idx]);
        StoreU(lin_r, d, &tmp_lin_r[idx]);
        StoreU(lin_g, d, &tmp_lin_g[idx]);
        StoreU(lin_b, d, &tmp_lin_b[idx]);
      }
      for (; x < xsize_padded; ++x) {
        size_t idx = y * xsize_padded + x;
        float py = best_y[idx];
        float ucb = up_cb[idx];
        float ucr = up_cr[idx];
        float r, g, b;
        YCbCrToRGB_Scalar(py, ucb, ucr, &r, &g, &b);
        float lr = GammaToLinear(r);
        float lg = GammaToLinear(g);
        float lb = GammaToLinear(b);
        float ly = 0.299f * lr + 0.587f * lg + 0.114f * lb;
        err_y[idx] = target_y[idx] - LinearToGamma(ly);
        tmp_lin_r[idx] = lr;
        tmp_lin_g[idx] = lg;
        tmp_lin_b[idx] = lb;
      }
    }

    // Accumulate |err_y|
    auto acc_y = Zero(d);
    size_t i_y = 0;
    for (; i_y + N <= high_size; i_y += N) {
      acc_y = Add(acc_y, Abs(LoadU(d, &err_y[i_y])));
    }
    diff_y_sum = ReduceSum(d, acc_y);
    for (; i_y < high_size; ++i_y) {
      diff_y_sum += std::abs(err_y[i_y]);
    }

    // 3c. Box-average downsample of reconstructed RGB -> compute Cb/Cr error
    //     (SIMD via BoxAverageDownsampleRow)
    for (size_t y = 0; y < down_height; ++y) {
      const float* r0 = &tmp_lin_r[(y * 2 + 0) * xsize_padded];
      const float* g0 = &tmp_lin_g[(y * 2 + 0) * xsize_padded];
      const float* b0 = &tmp_lin_b[(y * 2 + 0) * xsize_padded];
      const float* r1 = &tmp_lin_r[(y * 2 + 1) * xsize_padded];
      const float* g1 = &tmp_lin_g[(y * 2 + 1) * xsize_padded];
      const float* b1 = &tmp_lin_b[(y * 2 + 1) * xsize_padded];

      float* row_err_cb = &err_cb[y * down_width];
      float* row_err_cr = &err_cr[y * down_width];
      float* row_tgt_cb = &target_cb[y * down_width];
      float* row_tgt_cr = &target_cr[y * down_width];

      // BoxAverageDownsampleRow outputs Cb/Cr directly, so we use err
      // buffers as temp storage for the reconstructed Cb/Cr.
      BoxAverageDownsampleRow(r0, g0, b0, r1, g1, b1, down_width,
                              xsize_padded, row_err_cb, row_err_cr);

      // Compute error = target - reconstructed
      size_t x = 0;
      for (; x + N <= down_width; x += N) {
        auto tgt_cb = LoadU(d, row_tgt_cb + x);
        auto rec_cb = LoadU(d, row_err_cb + x);
        StoreU(Sub(tgt_cb, rec_cb), d, row_err_cb + x);

        auto tgt_cr = LoadU(d, row_tgt_cr + x);
        auto rec_cr = LoadU(d, row_err_cr + x);
        StoreU(Sub(tgt_cr, rec_cr), d, row_err_cr + x);
      }
      for (; x < down_width; ++x) {
        row_err_cb[x] = row_tgt_cb[x] - row_err_cb[x];
        row_err_cr[x] = row_tgt_cr[x] - row_err_cr[x];
      }
    }

    // 3d. Apply Y error correction with clamping (SIMD + scalar tail)
    const auto vmin = Set(d, 0.0f);
    const auto vmax = Set(d, 255.0f);
    size_t i_y_corr = 0;
    for (; i_y_corr + N <= high_size; i_y_corr += N) {
      auto y_vec = LoadU(d, &best_y[i_y_corr]);
      auto e_y_vec = LoadU(d, &err_y[i_y_corr]);
      auto res = Min(Max(Add(y_vec, e_y_vec), vmin), vmax);
      StoreU(res, d, &best_y[i_y_corr]);
    }
    for (; i_y_corr < high_size; ++i_y_corr) {
      best_y[i_y_corr] = std::max(0.0f, std::min(255.0f, best_y[i_y_corr] + err_y[i_y_corr]));
    }

    // 3e. Apply Cb/Cr error correction (SIMD + scalar tail)
    //     Unlike Y, Cb/Cr are NOT clamped during iterations to allow error diffusion.
    size_t i_c_corr = 0;
    for (; i_c_corr + N <= low_size; i_c_corr += N) {
      auto cb_vec = Add(LoadU(d, &best_cb[i_c_corr]), LoadU(d, &err_cb[i_c_corr]));
      auto cr_vec = Add(LoadU(d, &best_cr[i_c_corr]), LoadU(d, &err_cr[i_c_corr]));
      StoreU(cb_vec, d, &best_cb[i_c_corr]);
      StoreU(cr_vec, d, &best_cr[i_c_corr]);
    }
    for (; i_c_corr < low_size; ++i_c_corr) {
      best_cb[i_c_corr] += err_cb[i_c_corr];
      best_cr[i_c_corr] += err_cr[i_c_corr];
    }

    if (iter > 0 &&
        (diff_y_sum < diff_y_threshold || diff_y_sum > prev_diff_y_sum))
      break;
    prev_diff_y_sum = diff_y_sum;
  }

  // 4. Write best_y and best_cb/cr to raw_data output buffers (memcpy)
  for (size_t y = 0; y < iMCU_height; ++y) {
    float* row_out_y = m->raw_data[0]->Row(y0 + y);
    memcpy(row_out_y, &best_y[y * xsize_padded],
           xsize_padded * sizeof(float));
  }

  const size_t y_out0 = y0 / 2;
  for (size_t y_out = 0; y_out < down_height; ++y_out) {
    float* row_out_cb = m->raw_data[1]->Row(y_out0 + y_out);
    float* row_out_cr = m->raw_data[2]->Row(y_out0 + y_out);
    memcpy(row_out_cb, &best_cb[y_out * down_width],
           down_width * sizeof(float));
    memcpy(row_out_cr, &best_cr[y_out * down_width],
           down_width * sizeof(float));
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
