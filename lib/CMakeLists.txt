# Copyright (c) the JPEG XL Project Authors.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

set(JPEGLI_MAJOR_VERSION 0)
set(JPEGLI_MINOR_VERSION 12)
set(JPEGLI_PATCH_VERSION 0)
set(JPEGLI_LIBRARY_VERSION
    "${JPEGLI_MAJOR_VERSION}.${JPEGLI_MINOR_VERSION}.${JPEGLI_PATCH_VERSION}")

# This is the library API/ABI compatibility version. Changing this value makes
# the shared library incompatible with previous version. A program linked
# against this shared library SOVERSION will not run with an older SOVERSION.
# It is important to update this value when making incompatible API/ABI changes
# so that programs that depend on libjpegli can update their dependencies. Semantic
# versioning allows 0.y.z to have incompatible changes in minor versions.
set(JPEGLI_SO_MINOR_VERSION 12)
if (JPEGLI_MAJOR_VERSION EQUAL 0)
  set(JPEGLI_LIBRARY_SOVERSION
      "${JPEGLI_MAJOR_VERSION}.${JPEGLI_SO_MINOR_VERSION}")
else()
  set(JPEGLI_LIBRARY_SOVERSION "${JPEGLI_MAJOR_VERSION}")
endif()

# List of warning and feature flags for our library and tests.
if (MSVC)
  set(JPEGLI_INTERNAL_FLAGS
    # TODO(janwas): add flags
  )
else ()
  set(JPEGLI_INTERNAL_FLAGS
    # F_FLAGS
    -fmerge-all-constants
    -fno-builtin-fwrite
    -fno-builtin-fread

    # WARN_FLAGS
    -Wall
    -Wextra
    -Wc++11-compat
    -Warray-bounds
    -Wformat-security
    -Wimplicit-fallthrough
    -Wno-register  # Needed by public headers in lcms
    -Wno-unused-function
    -Wno-unused-parameter
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wvla
  )

  if (NOT JPEGLI_ENABLE_COVERAGE AND NOT SANITIZER MATCHES ".san")
    # NB: use `-Rpass-analysis=stack-frame-layout -g` for investigation.
    # TODO(eustas): tighten
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      list(APPEND JPEGLI_INTERNAL_FLAGS -Wframe-larger-than=5632)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14.0)
      list(APPEND JPEGLI_INTERNAL_FLAGS -Wframe-larger-than=5632)
    endif()
  endif()

  # Warning flags supported by clang.
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND JPEGLI_INTERNAL_FLAGS
      -Wdeprecated-increment-bool
      # TODO(deymo): Add -Wextra-semi once we update third_party/highway.
      # -Wextra-semi
      -Wfloat-overflow-conversion
      -Wfloat-zero-conversion
      -Wfor-loop-analysis
      -Wgnu-redeclared-enum
      -Winfinite-recursion
      -Wliteral-conversion
      -Wno-c++98-compat
      -Wno-unused-command-line-argument
      -Wprivate-header
      -Wself-assign
      -Wshadow
      -Wstring-conversion
      -Wtautological-overlap-compare
      -Wthread-safety-analysis
      -Wundefined-func-template
      -Wunreachable-code
      -Wunused-comparison
    )
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.0)
      # for SKCMS on PPC
      list(APPEND HWY_FLAGS -Wno-deprecated-altivec-src-compat)
    endif()
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 5.0)
      list(APPEND HWY_FLAGS -Wc++2a-extensions)
    endif()
  endif()  # Clang

  if (WIN32)
    list(APPEND JPEGLI_INTERNAL_FLAGS
      -Wno-cast-align
      -Wno-double-promotion
      -Wno-float-equal
      -Wno-format-nonliteral
      -Wno-shadow
      -Wno-sign-conversion
      -Wno-zero-as-null-pointer-constant
    )

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      list(APPEND JPEGLI_INTERNAL_FLAGS
        -Wno-used-but-marked-unused
        -Wno-unused-template
        -Wno-unused-member-function
        -Wno-shadow-field-in-constructor
        -Wno-language-extension-token
        -Wno-global-constructors
        -Wno-c++98-compat-pedantic
      )
    endif()  # Clang
  else()  # WIN32
    list(APPEND JPEGLI_INTERNAL_FLAGS
      -fsized-deallocation
      -fno-exceptions

      # Language flags
      -fmath-errno
    )

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      list(APPEND JPEGLI_INTERNAL_FLAGS
        -fnew-alignment=8
        -fno-cxx-exceptions
        -fno-slp-vectorize
        -fno-vectorize

        -disable-free
        -disable-llvm-verifier
      )
    endif()  # Clang
  endif()  # WIN32
endif()  #!MSVC

# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=109740
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  list(APPEND JPEGLI_INTERNAL_FLAGS -Wno-overloaded-virtual)
endif()

if (JPEGLI_ENABLE_SKCMS)
  list(APPEND JPEGLI_INTERNAL_FLAGS -DJPEGLI_ENABLE_SKCMS=1)
endif ()

# strips the -internal suffix from all the elements in LIST
function(strip_internal OUTPUT_VAR LIB_LIST)
  foreach(lib IN LISTS ${LIB_LIST})
    string(REGEX REPLACE "-internal$" "" lib "${lib}")
    list(APPEND out_list "${lib}")
  endforeach()
  set(${OUTPUT_VAR} ${out_list} PARENT_SCOPE)
endfunction()

# set variables for jpegli_cms.cmake and jpegli.cmake
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(PKGCONFIG_TARGET_INCLUDES "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(PKGCONFIG_TARGET_INCLUDES "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(PKGCONFIG_TARGET_LIBS "${CMAKE_INSTALL_LIBDIR}")
else()
    set(PKGCONFIG_TARGET_LIBS "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()

include(CheckCXXSymbolExists)
set(PKGCONFIG_CXX_LIB "")
check_cxx_symbol_exists(__GLIBCXX__ iostream LIBSTDCXX)
check_cxx_symbol_exists(_LIBCPP_VERSION iostream LIBCXX)
if(LIBSTDCXX)
  set(PKGCONFIG_CXX_LIB "-lstdc++")
elseif(LIBCXX)
  set(PKGCONFIG_CXX_LIB "-lc++")
endif()

add_library(jpegli_base INTERFACE)
target_include_directories(jpegli_base SYSTEM BEFORE INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
)
target_include_directories(jpegli_base BEFORE INTERFACE
  ${PROJECT_SOURCE_DIR}
  ${JPEGLI_HWY_INCLUDE_DIRS}
)

# On android, link with log to use android-related log functions.
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  find_library(log-lib log)
  if(log-lib)
    target_link_libraries(jpegli_base INTERFACE ${log-lib})
    target_compile_definitions(jpegli_base INTERFACE USE_ANDROID_LOGGER)
  endif()
endif()

# The jpegli_cms library definition.
include(jpegli_cms.cmake)

# Other libraries outside the core jpegli library.
if(JPEGLI_ENABLE_TOOLS OR BUILD_TESTING)
  include(jpegli_extras.cmake)
endif()
include(jpegli_threads.cmake)
include(jpegli.cmake)

# For simplicity all the library headers, both source and generated ones, are
# gathered in the binary folder. There is no distinction on which libraries use
# which header since it is expected that all developer libraries are available
# together at build time.
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/jpegli
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# Tests for the jpegli library.
include(jpegli_tests.cmake)
