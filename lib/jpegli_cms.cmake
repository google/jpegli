# Copyright (c) the JPEG XL Project Authors.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

include(jpegli_lists.cmake)

# Headers for exporting/importing public headers
include(GenerateExportHeader)

add_library(jpegli_cms
  ${JPEGLI_INTERNAL_CMS_SOURCES}
)
target_compile_options(jpegli_cms PRIVATE "${JPEGLI_INTERNAL_FLAGS}")
set_target_properties(jpegli_cms PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN 1)
target_link_libraries(jpegli_cms PUBLIC jpegli_base)
target_include_directories(jpegli_cms PRIVATE
  ${JPEGLI_HWY_INCLUDE_DIRS}
)
generate_export_header(jpegli_cms
  BASE_NAME JPEGLI_CMS
  EXPORT_FILE_NAME include/jpegli/jpegli_cms_export.h)
target_compile_definitions(jpegli_cms PUBLIC
  "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:JPEGLI_CMS_STATIC_DEFINE>")
target_include_directories(jpegli_cms BEFORE PUBLIC
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>")

set(JPEGLI_CMS_LIBRARY_REQUIRES "")

if (JPEGLI_ENABLE_SKCMS)
  target_link_skcms(jpegli_cms)
else()
  target_link_libraries(jpegli_cms PRIVATE lcms2)
  if (JPEGLI_FORCE_SYSTEM_LCMS2)
    set(JPEGLI_CMS_LIBRARY_REQUIRES "lcms2")
  endif()
endif()

target_link_libraries(jpegli_cms PRIVATE hwy)

set_target_properties(jpegli_cms PROPERTIES
        VERSION ${JPEGLI_LIBRARY_VERSION}
        SOVERSION ${JPEGLI_LIBRARY_SOVERSION})

# Check whether the linker support excluding libs
if (MSVC)
  # MSVC ignores this flag (with a warning), so CMake thinks it supports that.
  set(LINKER_EXCLUDE_LIBS_FLAG "")
  set(LINKER_SUPPORT_EXCLUDE_LIBS FALSE)
else()
  set(LINKER_EXCLUDE_LIBS_FLAG "-Wl,--exclude-libs=ALL")
  include(CheckCSourceCompiles)
  list(APPEND CMAKE_REQUIRED_LINK_OPTIONS ${LINKER_EXCLUDE_LIBS_FLAG})
  check_c_source_compiles("int main(){return 0;}" LINKER_SUPPORT_EXCLUDE_LIBS)
  list(REMOVE_ITEM CMAKE_REQUIRED_LINK_OPTIONS ${LINKER_EXCLUDE_LIBS_FLAG})
endif()

if(LINKER_SUPPORT_EXCLUDE_LIBS)
  set_property(TARGET jpegli_cms APPEND_STRING PROPERTY
      LINK_FLAGS " ${LINKER_EXCLUDE_LIBS_FLAG}")
endif()

install(TARGETS jpegli_cms
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (BUILD_SHARED_LIBS)
  set(JPEGLI_REQUIRES_TYPE "Requires.private")
  set(JPEGLI_CMS_PRIVATE_LIBS "-lm ${PKGCONFIG_CXX_LIB}")
else()
  set(JPEGLI_REQUIRES_TYPE "Requires")
  set(JPEGLI_CMS_PRIVATE_LIBS "-lm ${PKGCONFIG_CXX_LIB}")
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cms/libjpegli_cms.pc.in"
               "libjpegli_cms.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libjpegli_cms.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
