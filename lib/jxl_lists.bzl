# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

"""
This file is generated, do not modify by manually.
Run `tools/scripts/build_cleaner.py --update` to regenerate it.
"""

libjxl_base_sources = [
    "base/bits.h",
    "base/byte_order.h",
    "base/c_callback_support.h",
    "base/common.h",
    "base/compiler_specific.h",
    "base/data_parallel.h",
    "base/fast_math-inl.h",
    "base/float.h",
    "base/include_jpeglib.h",
    "base/matrix_ops.h",
    "base/memory_manager.h",
    "base/os_macros.h",
    "base/override.h",
    "base/parallel_runner.h",
    "base/printf_macros.h",
    "base/random.h",
    "base/rational_polynomial-inl.h",
    "base/rect.h",
    "base/sanitizer_definitions.h",
    "base/sanitizers.h",
    "base/span.h",
    "base/status.h",
    "base/testing.h",
    "base/types.h",
]

libjxl_cms_sources = [
    "cms/cms.h",
    "cms/cms_interface.h",
    "cms/color_encoding.h",
    "cms/color_encoding_cms.h",
    "cms/color_encoding_internal.h",
    "cms/jxl_cms.cc",
    "cms/jxl_cms_internal.h",
    "cms/opsin_params.h",
    "cms/tone_mapping-inl.h",
    "cms/tone_mapping.h",
    "cms/transfer_functions-inl.h",
    "cms/transfer_functions.h",
]

libjxl_codec_apng_sources = [
    "extras/dec/apng.h",
    "extras/enc/apng.h",
]

libjxl_codec_exr_sources = [
    "extras/dec/exr.cc",
    "extras/dec/exr.h",
    "extras/enc/exr.cc",
    "extras/enc/exr.h",
]

libjxl_codec_gif_sources = [
    "extras/dec/gif.cc",
    "extras/dec/gif.h",
]

libjxl_codec_jpegli_sources = [
    "extras/dec/jpegli.cc",
    "extras/dec/jpegli.h",
    "extras/enc/jpegli.cc",
    "extras/enc/jpegli.h",
]

libjxl_codec_jpg_sources = [
    "extras/dec/jpg.cc",
    "extras/dec/jpg.h",
    "extras/enc/jpg.cc",
    "extras/enc/jpg.h",
]

libjxl_codec_npy_sources = [
    "extras/enc/npy.cc",
    "extras/enc/npy.h",
]

libjxl_codec_pgx_sources = [
    "extras/dec/pgx.cc",
    "extras/dec/pgx.h",
    "extras/enc/pgx.cc",
    "extras/enc/pgx.h",
]

libjxl_codec_pnm_sources = [
    "extras/dec/pnm.cc",
    "extras/dec/pnm.h",
    "extras/enc/pnm.cc",
    "extras/enc/pnm.h",
]

libjxl_extras_for_tools_sources = [
    "extras/cache_aligned.cc",
    "extras/cache_aligned.h",
    "extras/butteraugli.cc",
    "extras/butteraugli.h",
    "extras/convolve-inl.h",
    "extras/convolve.h",
    "extras/convolve_separable5.cc",
    "extras/convolve_slow.cc",
    "extras/image.cc",
    "extras/image.h",
    "extras/image_color_transform.cc",
    "extras/image_color_transform.h",
    "extras/image_ops.h",
    "extras/metrics.cc",
    "extras/metrics.h",
    "extras/packed_image_convert.cc",
    "extras/packed_image_convert.h",
    "extras/simd_util.cc",
    "extras/simd_util.h",
    "extras/xyb_transform.cc",
    "extras/xyb_transform.h",
]

libjxl_extras_sources = [
    "extras/alpha_blend.cc",
    "extras/alpha_blend.h",
    "extras/codestream_header.h",
    "extras/dec/color_description.cc",
    "extras/dec/color_description.h",
    "extras/dec/color_hints.cc",
    "extras/dec/color_hints.h",
    "extras/dec/decode.cc",
    "extras/dec/decode.h",
    "extras/enc/encode.cc",
    "extras/enc/encode.h",
    "extras/exif.cc",
    "extras/exif.h",
    "extras/mmap.cc",
    "extras/mmap.h",
    "extras/packed_image.h",
    "extras/size_constraints.h",
    "extras/time.cc",
    "extras/time.h",
]

libjxl_gbench_sources = [
    "extras/tone_mapping_gbench.cc",
    "jxl/dec_external_image_gbench.cc",
    "jxl/enc_external_image_gbench.cc",
    "jxl/splines_gbench.cc",
    "jxl/tf_gbench.cc",
]

libjxl_jpegli_lib_version = 62

libjxl_jpegli_libjpeg_helper_files = [
    "jpegli/libjpeg_test_util.cc",
    "jpegli/libjpeg_test_util.h",
]

libjxl_jpegli_sources = [
    "jpegli/adaptive_quantization.cc",
    "jpegli/adaptive_quantization.h",
    "jpegli/bit_writer.cc",
    "jpegli/bit_writer.h",
    "jpegli/bitstream.cc",
    "jpegli/bitstream.h",
    "jpegli/color_quantize.cc",
    "jpegli/color_quantize.h",
    "jpegli/color_transform.cc",
    "jpegli/color_transform.h",
    "jpegli/common.cc",
    "jpegli/common.h",
    "jpegli/common_internal.h",
    "jpegli/dct-inl.h",
    "jpegli/decode.cc",
    "jpegli/decode.h",
    "jpegli/decode_internal.h",
    "jpegli/decode_marker.cc",
    "jpegli/decode_marker.h",
    "jpegli/decode_scan.cc",
    "jpegli/decode_scan.h",
    "jpegli/destination_manager.cc",
    "jpegli/downsample.cc",
    "jpegli/downsample.h",
    "jpegli/encode.cc",
    "jpegli/encode.h",
    "jpegli/encode_finish.cc",
    "jpegli/encode_finish.h",
    "jpegli/encode_internal.h",
    "jpegli/encode_streaming.cc",
    "jpegli/encode_streaming.h",
    "jpegli/entropy_coding-inl.h",
    "jpegli/entropy_coding.cc",
    "jpegli/entropy_coding.h",
    "jpegli/error.cc",
    "jpegli/error.h",
    "jpegli/huffman.cc",
    "jpegli/huffman.h",
    "jpegli/idct.cc",
    "jpegli/idct.h",
    "jpegli/input.cc",
    "jpegli/input.h",
    "jpegli/memory_manager.cc",
    "jpegli/memory_manager.h",
    "jpegli/quant.cc",
    "jpegli/quant.h",
    "jpegli/render.cc",
    "jpegli/render.h",
    "jpegli/simd.cc",
    "jpegli/simd.h",
    "jpegli/source_manager.cc",
    "jpegli/transpose-inl.h",
    "jpegli/types.h",
    "jpegli/upsample.cc",
    "jpegli/upsample.h",
]

libjxl_jpegli_testlib_files = [
    "jpegli/test_params.h",
    "jpegli/test_utils-inl.h",
    "jpegli/test_utils.cc",
    "jpegli/test_utils.h",
]

libjxl_jpegli_tests = [
    "jpegli/decode_api_test.cc",
    "jpegli/encode_api_test.cc",
    "jpegli/error_handling_test.cc",
    "jpegli/input_suspension_test.cc",
    "jpegli/output_suspension_test.cc",
    "jpegli/source_manager_test.cc",
    "jpegli/streaming_test.cc",
    "jpegli/transcode_api_test.cc",
]

libjxl_jpegli_wrapper_sources = [
    "jpegli/libjpeg_wrapper.cc",
]

libjxl_major_version = 0

libjxl_minor_version = 10

libjxl_patch_version = 2

libjxl_public_headers = [
    "include/jxl/cms.h",
    "include/jxl/cms_interface.h",
    "include/jxl/codestream_header.h",
    "include/jxl/color_encoding.h",
    "include/jxl/decode.h",
    "include/jxl/decode_cxx.h",
    "include/jxl/encode.h",
    "include/jxl/encode_cxx.h",
    "include/jxl/memory_manager.h",
    "include/jxl/parallel_runner.h",
    "include/jxl/stats.h",
    "include/jxl/types.h",
]

libjxl_testlib_files = [
    "extras/test_image.cc",
    "extras/test_image.h",
    "extras/test_utils.cc",
    "extras/test_utils.h",
    "threads/test_utils.h",
]

libjxl_tests = [
    "extras/butteraugli_test.cc",
    "extras/codec_test.cc",
    "extras/dec/color_description_test.cc",
    "extras/jpegli_test.cc",
    "threads/thread_parallel_runner_test.cc",
]

libjxl_threads_sources = [
    "threads/thread_parallel_runner.cc",
    "threads/thread_parallel_runner_internal.cc",
    "threads/thread_parallel_runner_internal.h",
    "threads/thread_parallel_runner.h",
    "threads/thread_parallel_runner_cxx.h",
]
