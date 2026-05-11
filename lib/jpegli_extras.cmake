# Copyright (c) the JPEG XL Project Authors.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

include(jpegli_lists.cmake)

# Object library for those parts of extras that do not depend on jpegli internals
# or jpegli. We will create two versions of these object files, one with and one
# without external codec support compiled in.
list(APPEND JPEGLI_EXTRAS_CORE_SOURCES
  "${JPEGLI_INTERNAL_EXTRAS_SOURCES}"
  "${JPEGLI_INTERNAL_CODEC_APNG_SOURCES}"
  ../third_party/apngdis/dec.cc
  ../third_party/apngdis/enc.cc
  "${JPEGLI_INTERNAL_CODEC_EXR_SOURCES}"
  "${JPEGLI_INTERNAL_CODEC_GIF_SOURCES}"
  "${JPEGLI_INTERNAL_CODEC_JPG_SOURCES}"
  "${JPEGLI_INTERNAL_CODEC_PGX_SOURCES}"
  "${JPEGLI_INTERNAL_CODEC_PNM_SOURCES}"
  "${JPEGLI_INTERNAL_CODEC_NPY_SOURCES}"
)
foreach(LIB jpegli_extras_core-obj jpegli_extras_core_nocodec-obj)
  add_library("${LIB}" OBJECT "${JPEGLI_EXTRAS_CORE_SOURCES}")
  list(APPEND JPEGLI_EXTRAS_OBJECT_LIBRARIES "${LIB}")
endforeach()
list(APPEND JPEGLI_EXTRAS_OBJECTS $<TARGET_OBJECTS:jpegli_extras_core-obj>)

# Object library for those parts of extras that depend on jpegli internals.
add_library(jpegli_extras_internal-obj OBJECT
  "${JPEGLI_INTERNAL_EXTRAS_FOR_TOOLS_SOURCES}"
)
list(APPEND JPEGLI_EXTRAS_OBJECT_LIBRARIES jpegli_extras_internal-obj)
list(APPEND JPEGLI_EXTRAS_OBJECTS $<TARGET_OBJECTS:jpegli_extras_internal-obj>)

set(JPEGLI_EXTRAS_CODEC_INTERNAL_LIBRARIES)

find_package(GIF 5.1)
if(GIF_FOUND)
  target_include_directories(jpegli_extras_core-obj PRIVATE "${GIF_INCLUDE_DIRS}")
  target_compile_definitions(jpegli_extras_core-obj PRIVATE -DJPEGLI_ENABLE_GIF=1)
  list(APPEND JPEGLI_EXTRAS_CODEC_INTERNAL_LIBRARIES ${GIF_LIBRARIES})
  if(JPEGLI_DEP_LICENSE_DIR)
    configure_file("${JPEGLI_DEP_LICENSE_DIR}/libgif-dev/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.libgif COPYONLY)
  endif()  # JPEGLI_DEP_LICENSE_DIR
endif()

find_package(JPEG)
if(JPEG_FOUND)
  target_include_directories(jpegli_extras_core-obj PRIVATE "${JPEG_INCLUDE_DIRS}")
  target_compile_definitions(jpegli_extras_core-obj PRIVATE -DJPEGLI_ENABLE_JPEG=1)
  list(APPEND JPEGLI_EXTRAS_CODEC_INTERNAL_LIBRARIES ${JPEG_LIBRARIES})
  if(JPEGLI_DEP_LICENSE_DIR)
    configure_file("${JPEGLI_DEP_LICENSE_DIR}/libjpeg-dev/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.libjpeg COPYONLY)
  endif()  # JPEGLI_DEP_LICENSE_DIR
endif()

if (JPEGLI_ENABLE_SJPEG)
  target_compile_definitions(jpegli_extras_core-obj PRIVATE
    -DJPEGLI_ENABLE_SJPEG=1)
  target_include_directories(jpegli_extras_core-obj PRIVATE
    ../third_party/sjpeg/src)
  list(APPEND JPEGLI_EXTRAS_CODEC_INTERNAL_LIBRARIES sjpeg)
endif()

add_library(jpegli_extras_jpegli-obj OBJECT
  "${JPEGLI_INTERNAL_CODEC_JPEGLI_SOURCES}"
)
target_include_directories(jpegli_extras_jpegli-obj PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/include/jpegli"
)
list(APPEND JPEGLI_EXTRAS_OBJECT_LIBRARIES jpegli_extras_jpegli-obj)
list(APPEND JPEGLI_EXTRAS_OBJECTS $<TARGET_OBJECTS:jpegli_extras_jpegli-obj>)

if(NOT JPEGLI_BUNDLE_LIBPNG)
  find_package(PNG)
endif()
if(PNG_FOUND)
  target_include_directories(jpegli_extras_core-obj PRIVATE "${PNG_INCLUDE_DIRS}")
  target_compile_definitions(jpegli_extras_core-obj PRIVATE -DJPEGLI_ENABLE_APNG=1)
  list(APPEND JPEGLI_EXTRAS_CODEC_INTERNAL_LIBRARIES ${PNG_LIBRARIES})
  configure_file(../third_party/apngdis/LICENSE
                 ${PROJECT_BINARY_DIR}/LICENSE.apngdis COPYONLY)
endif()

if (JPEGLI_ENABLE_OPENEXR)
  pkg_check_modules(OpenEXR IMPORTED_TARGET OpenEXR)
  if (OpenEXR_FOUND)
    target_include_directories(jpegli_extras_core-obj PRIVATE
      "${OpenEXR_INCLUDE_DIRS}"
    )
    target_compile_definitions(jpegli_extras_core-obj PRIVATE -DJPEGLI_ENABLE_EXR=1)
    list(APPEND JPEGLI_EXTRAS_CODEC_INTERNAL_LIBRARIES PkgConfig::OpenEXR)
    if(JPEGLI_DEP_LICENSE_DIR)
      configure_file("${JPEGLI_DEP_LICENSE_DIR}/libopenexr-dev/copyright"
                    ${PROJECT_BINARY_DIR}/LICENSE.libopenexr COPYONLY)
    endif()  # JPEGLI_DEP_LICENSE_DIR
    # OpenEXR generates exceptions, so we need exception support to catch them.
    # Actually those flags counteract the ones set in JPEGLI_INTERNAL_FLAGS.
    if (NOT WIN32)
      set(_exr_flags "")
      # With "-fexceptions" + LTO GCC fails to link.
      if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # GCC does not support that
        set(_exr_flags "-fcxx-exceptions")
      endif()
      if ("${OpenEXR_VERSION}" VERSION_LESS "2.5.7")
        string(APPEND _exr_flags " -Wno-deprecated-copy")
      endif()
      set_source_files_properties(extras/dec/exr.cc extras/enc/exr.cc
        PROPERTIES COMPILE_FLAGS "${_exr_flags}"
      )
    endif() # WIN32
  else()
    message(WARNING "OpenEXR NOT found")
  endif() # OpenEXR_FOUND
endif() # JPEGLI_ENABLE_OPENEXR

# Common settings for the object libraries.
foreach(LIB ${JPEGLI_EXTRAS_OBJECT_LIBRARIES})
  target_compile_options("${LIB}" PRIVATE "${JPEGLI_INTERNAL_FLAGS}")
  target_compile_definitions("${LIB}" PRIVATE -DJPEGLI_EXPORT=)
  set_property(TARGET "${LIB}" PROPERTY POSITION_INDEPENDENT_CODE ON)
  target_include_directories("${LIB}" BEFORE PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${JPEGLI_HWY_INCLUDE_DIRS}
  )
endforeach()

# Define an extras library that does not have the image codecs, only the core
# extras code. This is needed for some of the fuzzers.
add_library(jpegli_extras_nocodec-internal STATIC EXCLUDE_FROM_ALL
  $<TARGET_OBJECTS:jpegli_extras_core_nocodec-obj>
  $<TARGET_OBJECTS:jpegli_extras_internal-obj>
)
target_link_libraries(jpegli_extras_nocodec-internal PRIVATE jpegli_threads)

# We only define a static library jpegli_extras since it uses internal parts of
# jpegli library which are not accessible from outside the library in the
# shared library case.
add_library(jpegli_extras-internal STATIC EXCLUDE_FROM_ALL ${JPEGLI_EXTRAS_OBJECTS})
target_link_libraries(jpegli_extras-internal PRIVATE
  ${JPEGLI_EXTRAS_CODEC_INTERNAL_LIBRARIES}
  jpegli_threads
)
target_link_libraries(jpegli_extras-internal PRIVATE jpegli-static)

### Library that does not depend on internal parts of jpegli library.
### Used by cjpegli and djpegli binaries.
add_library(jpegli_extras_codec STATIC
  $<TARGET_OBJECTS:jpegli_extras_core-obj>
)
target_link_libraries(jpegli_extras_codec PRIVATE
  ${JPEGLI_EXTRAS_CODEC_INTERNAL_LIBRARIES}
)
target_link_libraries(jpegli_extras_codec PUBLIC jpegli)
set_target_properties(jpegli_extras_codec PROPERTIES
  VERSION ${JPEGLI_LIBRARY_VERSION}
  SOVERSION ${JPEGLI_LIBRARY_SOVERSION}
)
install(TARGETS jpegli_extras_codec
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
