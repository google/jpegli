# Copyright (c) the JPEG XL Project Authors.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# This file is generated, do not modify by manually.
# Run `tools/scripts/build_cleaner.py --update` to regenerate it.
set(JPEGXL_INTERNAL_BASE_SOURCES
  jxl/base/arch_macros.h
  jxl/base/bits.h
  jxl/base/byte_order.h
  jxl/base/c_callback_support.h
  jxl/base/common.h
  jxl/base/compiler_specific.h
  jxl/base/data_parallel.h
  jxl/base/exif.h
  jxl/base/fast_math-inl.h
  jxl/base/float.h
  jxl/base/iaca.h
  jxl/base/include_jpeglib.h
  jxl/base/matrix_ops.h
  jxl/base/os_macros.h
  jxl/base/override.h
  jxl/base/printf_macros.h
  jxl/base/random.h
  jxl/base/rational_polynomial-inl.h
  jxl/base/rect.h
  jxl/base/sanitizer_definitions.h
  jxl/base/scope_guard.h
  jxl/base/span.h
  jxl/base/status.h
)

set(JPEGXL_INTERNAL_CMS_SOURCES
  jxl/cms/color_encoding_cms.h
  jxl/cms/jxl_cms.cc
  jxl/cms/jxl_cms_internal.h
  jxl/cms/opsin_params.h
  jxl/cms/tone_mapping-inl.h
  jxl/cms/tone_mapping.h
  jxl/cms/transfer_functions-inl.h
  jxl/cms/transfer_functions.h
)

set(JPEGXL_INTERNAL_CODEC_APNG_SOURCES
  ../third_party/apngdis/dec.cc
  extras/dec/apng.h
  ../third_party/apngdis/enc.cc
  extras/enc/apng.h
)

set(JPEGXL_INTERNAL_CODEC_EXR_SOURCES
  extras/dec/exr.cc
  extras/dec/exr.h
  extras/enc/exr.cc
  extras/enc/exr.h
)

set(JPEGXL_INTERNAL_CODEC_GIF_SOURCES
  extras/dec/gif.cc
  extras/dec/gif.h
)

set(JPEGXL_INTERNAL_CODEC_JPEGLI_SOURCES
  extras/dec/jpegli.cc
  extras/dec/jpegli.h
  extras/enc/jpegli.cc
  extras/enc/jpegli.h
)

set(JPEGXL_INTERNAL_CODEC_JPG_SOURCES
  extras/dec/jpg.cc
  extras/dec/jpg.h
  extras/enc/jpg.cc
  extras/enc/jpg.h
)

set(JPEGXL_INTERNAL_CODEC_NPY_SOURCES
  extras/enc/npy.cc
  extras/enc/npy.h
)

set(JPEGXL_INTERNAL_CODEC_PGX_SOURCES
  extras/dec/pgx.cc
  extras/dec/pgx.h
  extras/enc/pgx.cc
  extras/enc/pgx.h
)

set(JPEGXL_INTERNAL_CODEC_PNM_SOURCES
  extras/dec/pnm.cc
  extras/dec/pnm.h
  extras/enc/pnm.cc
  extras/enc/pnm.h
)

set(JPEGXL_INTERNAL_DEC_SOURCES
  jxl/cache_aligned.cc
  jxl/cache_aligned.h
  jxl/color_encoding_internal.cc
  jxl/color_encoding_internal.h
  jxl/common.h
  jxl/convolve-inl.h
  jxl/convolve.h
  jxl/convolve_separable5.cc
  jxl/convolve_slow.cc
  jxl/convolve_symmetric3.cc
  jxl/convolve_symmetric5.cc
  jxl/dec_external_image.cc
  jxl/dec_external_image.h
  jxl/image.cc
  jxl/image.h
  jxl/image_ops.h
  jxl/padded_bytes.h
  jxl/sanitizers.h
  jxl/simd_util-inl.h
  jxl/simd_util.cc
  jxl/simd_util.h
)

set(JPEGXL_INTERNAL_ENC_SOURCES
  jxl/enc_external_image.cc
  jxl/enc_external_image.h
  jxl/enc_image_bundle.cc
  jxl/enc_image_bundle.h
  jxl/enc_xyb.cc
  jxl/enc_xyb.h
)

set(JPEGXL_INTERNAL_EXTRAS_FOR_TOOLS_SOURCES
  extras/butteraugli.cc
  extras/butteraugli.h
  extras/codec.cc
  extras/codec.h
  extras/metrics.cc
  extras/metrics.h
  extras/packed_image_convert.cc
  extras/packed_image_convert.h
)

set(JPEGXL_INTERNAL_EXTRAS_SOURCES
  extras/alpha_blend.cc
  extras/alpha_blend.h
  extras/dec/color_description.cc
  extras/dec/color_description.h
  extras/dec/color_hints.cc
  extras/dec/color_hints.h
  extras/dec/decode.cc
  extras/dec/decode.h
  extras/enc/encode.cc
  extras/enc/encode.h
  extras/exif.cc
  extras/exif.h
  extras/mmap.cc
  extras/mmap.h
  extras/packed_image.h
  extras/size_constraints.h
  extras/time.cc
  extras/time.h
)

set(JPEGXL_INTERNAL_GBENCH_SOURCES
  jxl/dec_external_image_gbench.cc
  jxl/enc_external_image_gbench.cc
  jxl/splines_gbench.cc
  jxl/tf_gbench.cc
)

set(JPEGXL_INTERNAL_JPEGLI_LIBJPEG_HELPER_FILES
  jpegli/libjpeg_test_util.cc
  jpegli/libjpeg_test_util.h
)

set(JPEGXL_INTERNAL_JPEGLI_SOURCES
  jpegli/adaptive_quantization.cc
  jpegli/adaptive_quantization.h
  jpegli/bit_writer.cc
  jpegli/bit_writer.h
  jpegli/bitstream.cc
  jpegli/bitstream.h
  jpegli/color_quantize.cc
  jpegli/color_quantize.h
  jpegli/color_transform.cc
  jpegli/color_transform.h
  jpegli/common.cc
  jpegli/common.h
  jpegli/common_internal.h
  jpegli/dct-inl.h
  jpegli/decode.cc
  jpegli/decode.h
  jpegli/decode_internal.h
  jpegli/decode_marker.cc
  jpegli/decode_marker.h
  jpegli/decode_scan.cc
  jpegli/decode_scan.h
  jpegli/destination_manager.cc
  jpegli/downsample.cc
  jpegli/downsample.h
  jpegli/encode.cc
  jpegli/encode.h
  jpegli/encode_finish.cc
  jpegli/encode_finish.h
  jpegli/encode_internal.h
  jpegli/encode_streaming.cc
  jpegli/encode_streaming.h
  jpegli/entropy_coding-inl.h
  jpegli/entropy_coding.cc
  jpegli/entropy_coding.h
  jpegli/error.cc
  jpegli/error.h
  jpegli/huffman.cc
  jpegli/huffman.h
  jpegli/idct.cc
  jpegli/idct.h
  jpegli/input.cc
  jpegli/input.h
  jpegli/memory_manager.cc
  jpegli/memory_manager.h
  jpegli/quant.cc
  jpegli/quant.h
  jpegli/render.cc
  jpegli/render.h
  jpegli/simd.cc
  jpegli/simd.h
  jpegli/source_manager.cc
  jpegli/transpose-inl.h
  jpegli/types.h
  jpegli/upsample.cc
  jpegli/upsample.h
)

set(JPEGXL_INTERNAL_JPEGLI_TESTLIB_FILES
  jpegli/test_params.h
  jpegli/test_utils-inl.h
  jpegli/test_utils.cc
  jpegli/test_utils.h
)

set(JPEGXL_INTERNAL_JPEGLI_TESTS
  jpegli/decode_api_test.cc
  jpegli/encode_api_test.cc
  jpegli/error_handling_test.cc
  jpegli/input_suspension_test.cc
  jpegli/output_suspension_test.cc
  jpegli/source_manager_test.cc
  jpegli/streaming_test.cc
  jpegli/transcode_api_test.cc
)

set(JPEGXL_INTERNAL_JPEGLI_WRAPPER_SOURCES
  jpegli/libjpeg_wrapper.cc
)

set(JPEGXL_INTERNAL_PUBLIC_HEADERS
  include/jxl/cms.h
  include/jxl/cms_interface.h
  include/jxl/codestream_header.h
  include/jxl/color_encoding.h
  include/jxl/decode.h
  include/jxl/memory_manager.h
  include/jxl/parallel_runner.h
  include/jxl/types.h
)

set(JPEGXL_INTERNAL_TESTLIB_FILES
  jxl/image_test_utils.h
  jxl/test_image.cc
  jxl/test_image.h
  extras/test_utils.cc
  extras/test_utils.h
  threads/test_utils.h
)

set(JPEGXL_INTERNAL_TESTS
  extras/butteraugli_test.cc
  extras/codec_test.cc
  extras/dec/color_description_test.cc
  extras/jpegli_test.cc
  threads/thread_parallel_runner_test.cc
)

set(JPEGXL_INTERNAL_THREADS_PUBLIC_HEADERS
  include/jxl/thread_parallel_runner.h
  include/jxl/thread_parallel_runner_cxx.h
)

set(JPEGXL_INTERNAL_THREADS_SOURCES
  threads/thread_parallel_runner.cc
  threads/thread_parallel_runner_internal.cc
  threads/thread_parallel_runner_internal.h
)
