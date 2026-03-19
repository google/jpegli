# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

# Ubuntu focal ships with cmake 3.16.
cmake_minimum_required(VERSION 3.16...3.27)

project(LIBJXL_DOC LANGUAGES C CXX)

find_package(Doxygen OPTIONAL_COMPONENTS dot)

if(DOXYGEN_FOUND AND TARGET Doxygen::dot)
  set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib")
  set(DOXYGEN_GENERATE_HTML "YES")
  set(DOXYGEN_GENERATE_XML "YES")
  if(JPEGXL_WARNINGS_AS_ERRORS)
    set(DOXYGEN_WARN_AS_ERROR "YES")
  endif()
  set(DOXYGEN_QUIET "YES")
  doxygen_add_docs(doc
    "${CMAKE_CURRENT_SOURCE_DIR}/api.txt"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Generating C API documentation")

else()  # DOXYGEN_FOUND

  message(WARNING "Doxygen or Dot not installed; 'doc' target will FAIL")
  # Create a "doc" target for compatibility since "doc" is not otherwise added
  # to the build when doxygen is not installed.
  add_custom_target(doc false
    COMMENT "Error: Can't generate doc since Doxygen or Dot not installed.")

endif() # DOXYGEN_FOUND
