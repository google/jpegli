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

inline float RGBToLuma_Scalar(float r, float g, float b) {
  return 0.299f * r + 0.587f * g + 0.114f * b;
}

void ApplySharpYuvDownsampleImpl(j_compress_ptr cinfo) {
  jpeg_comp_master* m = cinfo->master;
  const size_t iMCU_height = DCTSIZE * cinfo->max_v_samp_factor;
  const size_t y0 = m->next_iMCU_row * iMCU_height;
  const size_t y1 = y0 + iMCU_height;
  const size_t xsize_padded = m->xsize_blocks * DCTSIZE;

  // Verify that Cb and Cr are 4:2:0 subsampled.
  if (cinfo->num_components < 3) return;
  const int h_factor = cinfo->max_h_samp_factor / cinfo->comp_info[1].h_samp_factor;
  const int v_factor = cinfo->max_v_samp_factor / cinfo->comp_info[1].v_samp_factor;
  if(h_factor != 2 || v_factor != 2) {
    // Fallback logic for unsupported standard subsampling bounds
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
  
  std::vector<float> target_y(xsize_padded * iMCU_height);
  std::vector<float> best_y(xsize_padded * iMCU_height);

  std::vector<float> target_cb(down_width * down_height);
  std::vector<float> target_cr(down_width * down_height);
  std::vector<float> best_cb(down_width * down_height);
  std::vector<float> best_cr(down_width * down_height);

  // 1. Initial High-Res state and 2x2 Box Downscale
  for (size_t y = 0; y < down_height; ++y) {
    const float* row_y0 = m->smooth_input[0]->Row(y0 + y * 2);
    const float* row_y1 = m->smooth_input[0]->Row(y0 + y * 2 + 1);
    const float* row_cb0 = m->smooth_input[1]->Row(y0 + y * 2);
    const float* row_cb1 = m->smooth_input[1]->Row(y0 + y * 2 + 1);
    const float* row_cr0 = m->smooth_input[2]->Row(y0 + y * 2);
    const float* row_cr1 = m->smooth_input[2]->Row(y0 + y * 2 + 1);

    for (size_t x = 0; x < down_width; ++x) {
      float lin_r_sum = 0, lin_g_sum = 0, lin_b_sum = 0;
      for (int iy = 0; iy < 2; ++iy) {
        for (int ix = 0; ix < 2; ++ix) {
          float py = (iy == 0 ? row_y0 : row_y1)[x * 2 + ix];
          float pcb = (iy == 0 ? row_cb0 : row_cb1)[x * 2 + ix];
          float pcr = (iy == 0 ? row_cr0 : row_cr1)[x * 2 + ix];

          float r, g, b;
          YCbCrToRGB_Scalar(py, pcb, pcr, &r, &g, &b);
          
          float lin_r = GammaToLinear(r);
          float lin_g = GammaToLinear(g);
          float lin_b = GammaToLinear(b);
          
          lin_r_sum += lin_r;
          lin_g_sum += lin_g;
          lin_b_sum += lin_b;
          
          // Libwebp-like logic: isolate intrinsic target Luma to preserve 
          // sharp color edge brightness metrics
          float lin_y = RGBToLuma_Scalar(lin_r, lin_g, lin_b);
          size_t hy = y * 2 + iy;
          size_t hx = x * 2 + ix;
          target_y[hy * xsize_padded + hx] = LinearToGamma(lin_y);
          best_y[hy * xsize_padded + hx] = py; // Initial Y is the raw unaltered Y
        }
      }
      lin_r_sum *= 0.25f;
      lin_g_sum *= 0.25f;
      lin_b_sum *= 0.25f;

      float avg_r = LinearToGamma(lin_r_sum);
      float avg_g = LinearToGamma(lin_g_sum);
      float avg_b = LinearToGamma(lin_b_sum);

      float avg_y, avg_cb, avg_cr;
      RGBToYCbCr_Scalar(avg_r, avg_g, avg_b, &avg_y, &avg_cb, &avg_cr);

      target_cb[y * down_width + x] = avg_cb;
      target_cr[y * down_width + x] = avg_cr;
      best_cb[y * down_width + x] = avg_cb;
      best_cr[y * down_width + x] = avg_cr;
    }
  }

  // 2. Iterative Back-Projection (4-pass Y/Cb/Cr coupled stabilization)
  float prev_diff_y_sum = 1e30f;
  const float diff_y_threshold = 3.0f * xsize_padded * iMCU_height;

  for (int iter = 0; iter < 4; ++iter) {
    std::vector<float> err_y(xsize_padded * iMCU_height);
    std::vector<float> err_cb(down_width * down_height);
    std::vector<float> err_cr(down_width * down_height);
    float diff_y_sum = 0.0f;

    for (size_t y = 0; y < down_height; ++y) {
      size_t y_prev = (y == 0) ? 0 : y - 1;
      size_t y_next = (y + 1 == down_height) ? y : y + 1;

      const float* prev_cb = &best_cb[y_prev * down_width];
      const float* cur_cb = &best_cb[y * down_width];
      const float* next_cb = &best_cb[y_next * down_width];

      const float* prev_cr = &best_cr[y_prev * down_width];
      const float* cur_cr = &best_cr[y * down_width];
      const float* next_cr = &best_cr[y_next * down_width];

      for (size_t x = 0; x < down_width; ++x) {
        size_t x_prev = (x == 0) ? 0 : x - 1;
        size_t x_next = (x + 1 == down_width) ? x : x + 1;

        auto upsample_bilerp = [](float center, float t, float b, float l,
                                  float r, float tl, float tr, float bl,
                                  float br, float res[4]) {
          res[0] = (center * 9.0f + t * 3.0f + l * 3.0f + tl) / 16.0f;
          res[1] = (center * 9.0f + t * 3.0f + r * 3.0f + tr) / 16.0f;
          res[2] = (center * 9.0f + b * 3.0f + l * 3.0f + bl) / 16.0f;
          res[3] = (center * 9.0f + b * 3.0f + r * 3.0f + br) / 16.0f;
        };

        float up_cb[4];
        upsample_bilerp(cur_cb[x], prev_cb[x], next_cb[x], cur_cb[x_prev],
                        cur_cb[x_next], prev_cb[x_prev], prev_cb[x_next],
                        next_cb[x_prev], next_cb[x_next], up_cb);

        float up_cr[4];
        upsample_bilerp(cur_cr[x], prev_cr[x], next_cr[x], cur_cr[x_prev],
                        cur_cr[x_next], prev_cr[x_prev], prev_cr[x_next],
                        next_cr[x_prev], next_cr[x_next], up_cr);

        float lin_r_sum = 0, lin_g_sum = 0, lin_b_sum = 0;
        for (int iy = 0; iy < 2; ++iy) {
          for (int ix = 0; ix < 2; ++ix) {
            int i = iy * 2 + ix;
            size_t hx = x * 2 + ix;
            size_t hy = y * 2 + iy;
            float py = best_y[hy * xsize_padded + hx];
            
            float r, g, b;
            YCbCrToRGB_Scalar(py, up_cb[i], up_cr[i], &r, &g, &b);
            
            float lin_r = GammaToLinear(r);
            float lin_g = GammaToLinear(g);
            float lin_b = GammaToLinear(b);
            
            // Record Y error based on simulated decoded linear light
            float lin_y = RGBToLuma_Scalar(lin_r, lin_g, lin_b);
            float dec_gamma_y = LinearToGamma(lin_y);
            float error_y = target_y[hy * xsize_padded + hx] - dec_gamma_y;
            err_y[hy * xsize_padded + hx] = error_y;
            diff_y_sum += std::abs(error_y);
            
            lin_r_sum += lin_r;
            lin_g_sum += lin_g;
            lin_b_sum += lin_b;
          }
        }
        lin_r_sum *= 0.25f;
        lin_g_sum *= 0.25f;
        lin_b_sum *= 0.25f;

        float avg_r = LinearToGamma(lin_r_sum);
        float avg_g = LinearToGamma(lin_g_sum);
        float avg_b = LinearToGamma(lin_b_sum);

        float avg_y, avg_cb, avg_cr;
        RGBToYCbCr_Scalar(avg_r, avg_g, avg_b, &avg_y, &avg_cb, &avg_cr);

        err_cb[y * down_width + x] = target_cb[y * down_width + x] - avg_cb;
        err_cr[y * down_width + x] = target_cr[y * down_width + x] - avg_cr;
      }
    }
    
    const size_t N = hwy::HWY_NAMESPACE::Lanes(d);
    
    // Apply High-Res Y Error with clipping
    for (size_t y = 0; y < iMCU_height; ++y) {
      size_t x = 0;
      float* y_ptr = &best_y[y * xsize_padded];
      const float* ey_ptr = &err_y[y * xsize_padded];
      auto v_zero = Set(d, 0.0f);
      auto v_max = Set(d, 255.0f);
      for (; x + N <= xsize_padded; x += N) {
         auto y_vec = LoadU(d, y_ptr + x);
         auto e_y_vec = LoadU(d, ey_ptr + x);
         auto res = Add(y_vec, e_y_vec);
         res = Min(Max(res, v_zero), v_max);
         StoreU(res, d, y_ptr + x);
      }
      for (; x < xsize_padded; ++x) {
         y_ptr[x] = std::max(0.0f, std::min(255.0f, y_ptr[x] + ey_ptr[x]));
      }
    }
    
    // Apply Cb/Cr Error
    for (size_t y = 0; y < down_height; ++y) {
      size_t x = 0;
      float* cb_ptr = &best_cb[y * down_width];
      float* cr_ptr = &best_cr[y * down_width];
      const float* ecb_ptr = &err_cb[y * down_width];
      const float* ecr_ptr = &err_cr[y * down_width];

      for (; x + N <= down_width; x += N) {
         auto cb_vec = LoadU(d, cb_ptr + x);
         auto cr_vec = LoadU(d, cr_ptr + x);
         auto e_cb_vec = LoadU(d, ecb_ptr + x);
         auto e_cr_vec = LoadU(d, ecr_ptr + x);
         StoreU(Add(cb_vec, e_cb_vec), d, cb_ptr + x);
         StoreU(Add(cr_vec, e_cr_vec), d, cr_ptr + x);
      }
      for (; x < down_width; ++x) {
         cb_ptr[x] += ecb_ptr[x];
         cr_ptr[x] += ecr_ptr[x];
      }
    }

    // test libwebp exit condition
    if (iter > 0) {
      if (diff_y_sum < diff_y_threshold) break;
      if (diff_y_sum > prev_diff_y_sum) break;
    }
    prev_diff_y_sum = diff_y_sum;
  }

  // 3. Write output to buffers
  const size_t N = hwy::HWY_NAMESPACE::Lanes(d);
  
  // Write Luma
  for (size_t y = 0; y < iMCU_height; ++y) {
    float* row_out_y = m->raw_data[0]->Row(y0 + y);
    float* src_y = &best_y[y * xsize_padded];
    size_t x = 0;
    for (; x + N <= xsize_padded; x += N) {
       StoreU(LoadU(d, src_y + x), d, row_out_y + x);
    }
    for (; x < xsize_padded; ++x) {
       row_out_y[x] = src_y[x];
    }
  }
  
  // Write Chroma
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
