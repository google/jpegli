// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "lib/jpegli/sharp_yuv.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "lib/jpegli/common.h"
#include "lib/jpegli/encode_internal.h"
#include "lib/jpegli/simd.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jpegli/sharp_yuv.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>
#include <hwy/contrib/math/math-inl.h>

HWY_BEFORE_NAMESPACE();
namespace jpegli {
namespace HWY_NAMESPACE {

using hwy::HWY_NAMESPACE::Add;
using hwy::HWY_NAMESPACE::Div;
using hwy::HWY_NAMESPACE::LoadU;
using hwy::HWY_NAMESPACE::Max;
using hwy::HWY_NAMESPACE::Min;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::MulAdd;
using hwy::HWY_NAMESPACE::Set;
using hwy::HWY_NAMESPACE::StoreU;
using hwy::HWY_NAMESPACE::Sub;
using hwy::HWY_NAMESPACE::IfThenElse;
using hwy::HWY_NAMESPACE::Le;

using D = HWY_CAPPED(float, 8);
constexpr D d;

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
    return (1.055f * std::pow(std::max(0.0f, v), 1.0f / 2.4f) - 0.055f) * 255.0f;
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

template <class D, class V>
HWY_INLINE V GammaToLinear_SIMD(D d, V v) {
  auto v_zero = Set(d, 0.0f);
  auto v_max = Set(d, 255.0f);
  auto n = Mul(Min(Max(v, v_zero), v_max), Set(d, 1.0f / 255.0f));
  auto is_low = Le(n, Set(d, 0.04045f));
  auto low_val = Div(n, Set(d, 12.92f));
  auto base = Div(Add(n, Set(d, 0.055f)), Set(d, 1.055f));
  base = Max(base, Set(d, 1e-6f));
  auto log_base = hwy::HWY_NAMESPACE::Log2(d, base);
  auto high_val = hwy::HWY_NAMESPACE::Exp2(d, Mul(log_base, Set(d, 2.4f)));
  return IfThenElse(is_low, low_val, high_val);
}

template <class D, class V>
HWY_INLINE V LinearToGamma_SIMD(D d, V v) {
  v = Max(v, Set(d, 0.0f));
  auto is_low = Le(v, Set(d, 0.0031308f));
  auto low_val = Mul(v, Set(d, 12.92f * 255.0f));
  auto v_safe = Max(v, Set(d, 1e-6f));
  auto log_v = hwy::HWY_NAMESPACE::Log2(d, v_safe);
  auto p = hwy::HWY_NAMESPACE::Exp2(d, Mul(log_v, Set(d, 1.0f / 2.4f)));
  auto high_val = Mul(Sub(Mul(p, Set(d, 1.055f)), Set(d, 0.055f)), Set(d, 255.0f));
  return IfThenElse(is_low, low_val, high_val);
}

template <class D, class V>
HWY_INLINE void YCbCrToRGB_SIMD(D d, V y, V cb, V cr, V* r, V* g, V* b) {
  auto v128 = Set(d, 128.0f);
  cb = Sub(cb, v128);
  cr = Sub(cr, v128);
  *r = Add(y, Mul(cr, Set(d, 1.402f)));
  *g = Add(y, Add(Mul(cb, Set(d, -0.114f * 1.772f / 0.587f)), Mul(cr, Set(d, -0.299f * 1.402f / 0.587f))));
  *b = Add(y, Mul(cb, Set(d, 1.772f)));
}

template <class D, class V>
HWY_INLINE V TempLuma_SIMD(D d, V r, V g, V b) {
  return Add(Mul(Set(d, 0.299f), r), Add(Mul(Set(d, 0.587f), g), Mul(Set(d, 0.114f), b)));
}

void ApplySharpYuvDownsampleImpl(j_compress_ptr cinfo) {
  jpeg_comp_master* m = cinfo->master;
  const size_t iMCU_height = DCTSIZE * cinfo->max_v_samp_factor;
  const size_t y0 = m->next_iMCU_row * iMCU_height;
  const size_t y1 = y0 + iMCU_height;
  const size_t xsize_padded = m->xsize_blocks * DCTSIZE;

  if (cinfo->num_components < 3) return;
  const int h_factor = cinfo->max_h_samp_factor / cinfo->comp_info[1].h_samp_factor;
  const int v_factor = cinfo->max_v_samp_factor / cinfo->comp_info[1].v_samp_factor;
  if(h_factor != 2 || v_factor != 2) {
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

  // 1. Initial High-Res state -> SIMD Loop
  for (size_t y = 0; y < iMCU_height; ++y) {
    const float* row_y = m->smooth_input[0]->Row(y0 + y);
    const float* row_cb = m->smooth_input[1]->Row(y0 + y);
    const float* row_cr = m->smooth_input[2]->Row(y0 + y);
    size_t x = 0;
    for (; x + N <= xsize_padded; x += N) {
      auto py = LoadU(d, row_y + x);
      auto pcb = LoadU(d, row_cb + x);
      auto pcr = LoadU(d, row_cr + x);
      
      auto r = Set(d,0.0f), g = Set(d,0.0f), b = Set(d,0.0f);
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
      float py = row_y[x], pcb = row_cb[x], pcr = row_cr[x];
      float r, g, b; YCbCrToRGB_Scalar(py, pcb, pcr, &r, &g, &b);
      float lr = GammaToLinear(r), lg = GammaToLinear(g), lb = GammaToLinear(b);
      float ly = 0.299 * lr + 0.587 * lg + 0.114 * lb;
      size_t idx = y * xsize_padded + x;
      target_y[idx] = LinearToGamma(ly);
      best_y[idx] = py;
      tmp_lin_r[idx] = lr; tmp_lin_g[idx] = lg; tmp_lin_b[idx] = lb;
    }
  }

  // 1b. Box downscale to target_cb, target_cr -> Scalar loop 
  // (Downscaled loops represent 1/4 the iterations, scalar overhead is negligible)
  for (size_t y = 0; y < down_height; ++y) {
    for (size_t x = 0; x < down_width; ++x) {
      float lr_sum = 0, lg_sum = 0, lb_sum = 0;
      for (int iy = 0; iy < 2; ++iy) {
        for (int ix = 0; ix < 2; ++ix) {
          size_t idx = (y * 2 + iy) * xsize_padded + (x * 2 + ix);
          lr_sum += tmp_lin_r[idx];
          lg_sum += tmp_lin_g[idx];
          lb_sum += tmp_lin_b[idx];
        }
      }
      float avg_r = LinearToGamma(lr_sum * 0.25f);
      float avg_g = LinearToGamma(lg_sum * 0.25f);
      float avg_b = LinearToGamma(lb_sum * 0.25f);
      float avg_y, avg_cb, avg_cr;
      RGBToYCbCr_Scalar(avg_r, avg_g, avg_b, &avg_y, &avg_cb, &avg_cr);
      size_t di = y * down_width + x;
      target_cb[di] = avg_cb; target_cr[di] = avg_cr;
      best_cb[di] = avg_cb;   best_cr[di] = avg_cr;
    }
  }

  float prev_diff_y_sum = 1e30f;
  const float diff_y_threshold = 3.0f * high_size;

  for (int iter = 0; iter < 4; ++iter) {
    std::vector<float> err_y(high_size);
    std::vector<float> err_cb(low_size);
    std::vector<float> err_cr(low_size);
    float diff_y_sum = 0.0f;

    // Bilerp Up to Full Res (Scalar optimized loop)
    for (size_t y = 0; y < down_height; ++y) {
      size_t y_prev = (y == 0) ? 0 : y - 1;
      size_t y_next = (y + 1 == down_height) ? y : y + 1;
      for (size_t x = 0; x < down_width; ++x) {
        size_t x_prev = (x == 0) ? 0 : x - 1;
        size_t x_next = (x + 1 == down_width) ? x : x + 1;
        
        auto get_avg = [](float c, float t, float b, float l, float r, 
                          float tl, float tr, float bl, float br, float res[4]) {
          res[0] = (c * 9.0f + t * 3.0f + l * 3.0f + tl) / 16.0f;
          res[1] = (c * 9.0f + t * 3.0f + r * 3.0f + tr) / 16.0f;
          res[2] = (c * 9.0f + b * 3.0f + l * 3.0f + bl) / 16.0f;
          res[3] = (c * 9.0f + b * 3.0f + r * 3.0f + br) / 16.0f;
        };
        
        float u_cb[4], u_cr[4];
        float ccb = best_cb[y*down_width+x], ccr = best_cr[y*down_width+x];
        get_avg(ccb, best_cb[y_prev*down_width+x], best_cb[y_next*down_width+x],
                best_cb[y*down_width+x_prev], best_cb[y*down_width+x_next],
                best_cb[y_prev*down_width+x_prev], best_cb[y_prev*down_width+x_next],
                best_cb[y_next*down_width+x_prev], best_cb[y_next*down_width+x_next], u_cb);
        get_avg(ccr, best_cr[y_prev*down_width+x], best_cr[y_next*down_width+x],
                best_cr[y*down_width+x_prev], best_cr[y*down_width+x_next],
                best_cr[y_prev*down_width+x_prev], best_cr[y_prev*down_width+x_next],
                best_cr[y_next*down_width+x_prev], best_cr[y_next*down_width+x_next], u_cr);

        for(int iy=0; iy<2; ++iy) {
          for(int ix=0; ix<2; ++ix) {
            size_t idx = (y * 2 + iy) * xsize_padded + (x * 2 + ix);
            up_cb[idx] = u_cb[iy*2+ix];
            up_cr[idx] = u_cr[iy*2+ix];
          }
        }
      }
    }

    // SIMD Evaluate Error 
    for (size_t y = 0; y < iMCU_height; ++y) {
      size_t x = 0;
      for (; x + N <= xsize_padded; x += N) {
        size_t idx = y * xsize_padded + x;
        auto py = LoadU(d, &best_y[idx]);
        auto ucb = LoadU(d, &up_cb[idx]);
        auto ucr = LoadU(d, &up_cr[idx]);

        auto r = Set(d,0.0f), g = Set(d,0.0f), b = Set(d,0.0f);
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
        float py = best_y[idx], ucb = up_cb[idx], ucr = up_cr[idx];
        float r, g, b; YCbCrToRGB_Scalar(py, ucb, ucr, &r, &g, &b);
        float lr = GammaToLinear(r), lg = GammaToLinear(g), lb = GammaToLinear(b);
        float ly = 0.299 * lr + 0.587 * lg + 0.114 * lb;
        err_y[idx] = target_y[idx] - LinearToGamma(ly);
        tmp_lin_r[idx] = lr; tmp_lin_g[idx] = lg; tmp_lin_b[idx] = lb;
      }
    }
    
    for (size_t i = 0; i < high_size; ++i) diff_y_sum += std::abs(err_y[i]);

    // Box downscale decoupled backprojection Cb/Cr evaluation
    for (size_t y = 0; y < down_height; ++y) {
      for (size_t x = 0; x < down_width; ++x) {
        float lr_sum = 0, lg_sum = 0, lb_sum = 0;
        for (int iy = 0; iy < 2; ++iy) {
          for (int ix = 0; ix < 2; ++ix) {
            size_t idx = (y * 2 + iy) * xsize_padded + (x * 2 + ix);
            lr_sum += tmp_lin_r[idx];
            lg_sum += tmp_lin_g[idx];
            lb_sum += tmp_lin_b[idx];
          }
        }
        float avg_r = LinearToGamma(lr_sum * 0.25f);
        float avg_g = LinearToGamma(lg_sum * 0.25f);
        float avg_b = LinearToGamma(lb_sum * 0.25f);
        float avg_y, avg_cb, avg_cr;
        RGBToYCbCr_Scalar(avg_r, avg_g, avg_b, &avg_y, &avg_cb, &avg_cr);
        size_t di = y * down_width + x;
        err_cb[di] = target_cb[di] - avg_cb;
        err_cr[di] = target_cr[di] - avg_cr;
      }
    }
    
    // Apply Y Error
    for (size_t i = 0; i + N <= high_size; i += N) {
       auto y_vec = LoadU(d, &best_y[i]);
       auto e_y_vec = LoadU(d, &err_y[i]);
       auto res = Add(y_vec, e_y_vec);
       res = Min(Max(res, Set(d, 0.0f)), Set(d, 255.0f));
       StoreU(res, d, &best_y[i]);
    }
    for (size_t i = high_size - (high_size % N); i < high_size; ++i) {
       best_y[i] = std::max(0.0f, std::min(255.0f, best_y[i] + err_y[i]));
    }
    
    // Apply Cb/Cr Error
    for (size_t i = 0; i + N <= low_size; i += N) {
       StoreU(Add(LoadU(d, &best_cb[i]), LoadU(d, &err_cb[i])), d, &best_cb[i]);
       StoreU(Add(LoadU(d, &best_cr[i]), LoadU(d, &err_cr[i])), d, &best_cr[i]);
    }
    for (size_t i = low_size - (low_size % N); i < low_size; ++i) {
       best_cb[i] += err_cb[i];
       best_cr[i] += err_cr[i];
    }

    if (iter > 0 && (diff_y_sum < diff_y_threshold || diff_y_sum > prev_diff_y_sum)) break;
    prev_diff_y_sum = diff_y_sum;
  }

  // 3. Write output to buffers
  for (size_t y = 0; y < iMCU_height; ++y) {
    float* row_out_y = m->raw_data[0]->Row(y0 + y);
    float* src_y = &best_y[y * xsize_padded];
    size_t x = 0;
    for (; x + N <= xsize_padded; x += N) StoreU(LoadU(d, src_y + x), d, row_out_y + x);
    for (; x < xsize_padded; ++x) row_out_y[x] = src_y[x];
  }
  
  const size_t y_out0 = y0 / 2;
  for (size_t y_out = 0; y_out < down_height; ++y_out) {
    float* row_out_cb = m->raw_data[1]->Row(y_out0 + y_out);
    float* row_out_cr = m->raw_data[2]->Row(y_out0 + y_out);
    float* src_cb = &best_cb[y_out * down_width];
    float* src_cr = &best_cr[y_out * down_width];
    size_t x = 0;
    for (; x + N <= down_width; x += N) {
       StoreU(LoadU(d, src_cb + x), d, row_out_cb + x);
       StoreU(LoadU(d, src_cr + x), d, row_out_cr + x);
    }
    for (; x < down_width; ++x) {
       row_out_cb[x] = src_cb[x];
       row_out_cr[x] = src_cr[x];
    }
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
