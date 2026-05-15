# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

load("@bazel_skylib//rules:copy_file.bzl", "copy_file")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

# Load sources/headers/tests lists.
load(
    "jpegli_lists.bzl",
    "libjpegli_base_sources",
    "libjpegli_cms_sources",
    "libjpegli_codec_apng_sources",
    "libjpegli_codec_exr_sources",
    "libjpegli_codec_gif_sources",
    "libjpegli_codec_jpegli_sources",
    "libjpegli_codec_jpg_sources",
    "libjpegli_codec_npy_sources",
    "libjpegli_codec_pgx_sources",
    "libjpegli_codec_pnm_sources",
    "libjpegli_extras_for_tools_sources",
    "libjpegli_extras_sources",
    # "libjpegli_gbench_sources",
    # "libjpegli_jpegli_lib_version",
    "libjpegli_jpegli_libjpeg_helper_files",
    "libjpegli_jpegli_sources",
    "libjpegli_jpegli_testlib_files",
    "libjpegli_jpegli_tests",
    "libjpegli_testlib_files",
    "libjpegli_tests",
    "libjpegli_threads_sources",
)
load(
    "jpegli_vars.bzl",
    # "libjpegli_deps_testdata",
    # "libjpegli_deps_webp",
    "jpegli_root_package",
    "libjpegli_deps_exr",
    "libjpegli_deps_gif",
    "libjpegli_deps_gtest",
    "libjpegli_deps_hwy",
    "libjpegli_deps_hwy_nanobenchmark",
    "libjpegli_deps_hwy_test_util",
    "libjpegli_deps_jpeg",
    "libjpegli_deps_png",
    "libjpegli_deps_runfiles",
    "libjpegli_deps_skcms",
    "libjpegli_test_shards",
    "libjpegli_test_timeouts",
)

DEFAULT_VISIBILITY = ["//:__subpackages__"]

DEFAULT_COMPATIBILITY = []

INCLUDES_DIR = "include"

package(
    default_visibility = DEFAULT_VISIBILITY,
)

licenses(["notice"])

exports_files(["LICENSE"])

EXPORT_TEMPLATE = """
#ifndef @_EXPORT_H
#define @_EXPORT_H

#define @_EXPORT
#define @_NO_EXPORT

#ifndef @_DEPRECATED
#  define @_DEPRECATED __attribute__ ((__deprecated__))
#endif

#endif
"""

JPEGLI_CMS_EXPORT_H = INCLUDES_DIR + "/jpegli/jpegli_cms_export.h"

genrule(
    name = "create_jpegli_cms_export",
    outs = [JPEGLI_CMS_EXPORT_H],
    cmd = "echo '" + EXPORT_TEMPLATE.replace("@", "JPEGLI_CMS") + "' > $@",
    compatible_with = DEFAULT_COMPATIBILITY,
)

cc_library(
    name = "cms_includes",
    hdrs = [JPEGLI_CMS_EXPORT_H],
    compatible_with = DEFAULT_COMPATIBILITY,
    strip_include_prefix = INCLUDES_DIR,
)

JPEGLI_THREADS_EXPORT_H = INCLUDES_DIR + "/jpegli/jpegli_threads_export.h"

genrule(
    name = "create_jpegli_threads_export",
    outs = [JPEGLI_THREADS_EXPORT_H],
    cmd = "echo '" + EXPORT_TEMPLATE.replace("@", "JPEGLI_THREADS") + "' > $@",
    compatible_with = DEFAULT_COMPATIBILITY,
)

JPEGLI_JCONFIG_H = INCLUDES_DIR + "/jpegli/jconfig.h"

JPEGLI_JMORECFG_H = INCLUDES_DIR + "/jpegli/jmorecfg.h"

JPEGLI_JPEGLIB_H = INCLUDES_DIR + "/jpegli/jpeglib.h"

copy_file(
    name = "expand_jconfig",
    src = "@libjpeg_turbo//:jconfig.h",
    out = JPEGLI_JCONFIG_H,
    compatible_with = DEFAULT_COMPATIBILITY,
)

copy_file(
    name = "copy_jmorecfg",
    src = "@libjpeg_turbo//:jmorecfg.h",
    out = JPEGLI_JMORECFG_H,
    compatible_with = DEFAULT_COMPATIBILITY,
)

copy_file(
    name = "copy_jpeglib",
    src = "@libjpeg_turbo//:jpeglib.h",
    out = JPEGLI_JPEGLIB_H,
    compatible_with = DEFAULT_COMPATIBILITY,
)

cc_library(
    name = "libjpeg_includes",
    hdrs = [
        JPEGLI_JCONFIG_H,
        JPEGLI_JMORECFG_H,
        JPEGLI_JPEGLIB_H,
    ],
    compatible_with = DEFAULT_COMPATIBILITY,
    strip_include_prefix = INCLUDES_DIR + "/jpegli",
)

cc_library(
    name = "base",
    srcs = [path for path in libjpegli_base_sources if path.endswith(".cc")],
    hdrs = [path for path in libjpegli_base_sources if path.endswith(".h")],
    compatible_with = DEFAULT_COMPATIBILITY,
    deps = libjpegli_deps_hwy,
)

cc_library(
    name = "jpegli_threads",
    srcs = libjpegli_threads_sources,
    hdrs = [JPEGLI_THREADS_EXPORT_H],
    compatible_with = DEFAULT_COMPATIBILITY,
    strip_include_prefix = INCLUDES_DIR,
    deps = [
        ":base",
    ],
)

CODEC_FILES = libjpegli_codec_apng_sources + libjpegli_codec_exr_sources + libjpegli_codec_gif_sources + libjpegli_codec_jpegli_sources + libjpegli_codec_jpg_sources + libjpegli_codec_npy_sources + libjpegli_codec_pgx_sources + libjpegli_codec_pnm_sources

CODEC_SRCS = [path for path in CODEC_FILES if path.endswith(".cc")]

CODEC_HDRS = [path for path in CODEC_FILES if path.endswith(".h")]

cc_library(
    name = "jpegli",
    srcs = libjpegli_jpegli_sources,
    hdrs = [
        "jpegli/common_internal.h",  # TODO(eustas): should not be here
    ],
    compatible_with = DEFAULT_COMPATIBILITY,
    deps = [
        ":base",
        ":libjpeg_includes",
    ] + libjpegli_deps_hwy,
)

genrule(
    name = "copy_apngdis_sources",
    srcs = [
        "//third_party/apngdis:dec.cc",
        "//third_party/apngdis:enc.cc",
    ],
    outs = [
        "apngdis/dec.cc",
        "apngdis/enc.cc",
    ],
    cmd = "cp $(SRCS) $(RULEDIR)/apngdis",
)

# TODO(eustas): build codecs separately?
cc_library(
    name = "jpegli_extras",
    srcs = libjpegli_extras_sources + libjpegli_extras_for_tools_sources + libjpegli_cms_sources + CODEC_SRCS + [
        "apngdis/dec.cc",
        "apngdis/enc.cc",
    ],
    hdrs = CODEC_HDRS,
    compatible_with = DEFAULT_COMPATIBILITY,
    defines = [
        "JPEGLI_ENABLE_APNG=1",
        "JPEGLI_ENABLE_EXR=1",
        "JPEGLI_ENABLE_GIF=1",
        "JPEGLI_ENABLE_JPEG=1",
        "JPEGLI_ENABLE_SKCMS=1",
    ],
    deps = [
        ":cms_includes",
        ":jpegli",
        ":jpegli_threads",
    ] + libjpegli_deps_exr + libjpegli_deps_gif + libjpegli_deps_jpeg + libjpegli_deps_png + libjpegli_deps_skcms,
)

TESTLIB_FILES = libjpegli_testlib_files + libjpegli_jpegli_testlib_files + libjpegli_jpegli_libjpeg_helper_files

cc_library(
    name = "test_utils",
    testonly = 1,
    srcs = [path for path in TESTLIB_FILES if not path.endswith(".h")],
    hdrs = [path for path in TESTLIB_FILES if path.endswith(".h")],
    compatible_with = DEFAULT_COMPATIBILITY,
    defines = [
        'JPEGLI_ROOT_PACKAGE=\'"' + jpegli_root_package + '"\'',
    ],
    deps = [
        ":jpegli",
        ":jpegli_extras",
    ] + libjpegli_deps_runfiles,
)

TESTS = [path.partition(".")[0] for path in libjpegli_tests + libjpegli_jpegli_tests]

[
    cc_test(
        name = test,
        timeout = libjpegli_test_timeouts.get(test, "moderate"),
        srcs = [
            test + ".cc",
            "base/testing.h",
            "jpegli/testing.h",
        ],
        data = ["//:testdata"],
        shard_count = libjpegli_test_shards.get(test, 1),
        deps = [
            ":jpegli_extras",
            ":jpegli_threads",
            ":test_utils",
        ] + libjpegli_deps_gtest + libjpegli_deps_hwy_test_util + libjpegli_deps_hwy_nanobenchmark,
    )
    for test in TESTS
]
