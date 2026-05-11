# Copyright (c) the JPEG XL Project Authors.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

find_package(Threads REQUIRED)

include(jpegli_lists.cmake)

add_library(jpegli_threads ${JPEGLI_INTERNAL_THREADS_SOURCES})
target_compile_options(jpegli_threads PRIVATE ${JPEGLI_INTERNAL_FLAGS})
target_compile_options(jpegli_threads PUBLIC ${JPEGLI_COVERAGE_FLAGS})
set_property(TARGET jpegli_threads PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(jpegli_threads
  PRIVATE
    "${PROJECT_SOURCE_DIR}"
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/include")

target_link_libraries(jpegli_threads
  PUBLIC ${JPEGLI_COVERAGE_FLAGS} Threads::Threads
)

set_target_properties(jpegli_threads PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN 1
  DEFINE_SYMBOL JPEGLI_THREADS_INTERNAL_LIBRARY_BUILD
)

install(TARGETS jpegli_threads
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

set_target_properties(jpegli_threads PROPERTIES
  VERSION ${JPEGLI_LIBRARY_VERSION}
  SOVERSION ${JPEGLI_LIBRARY_SOVERSION})

set_target_properties(jpegli_threads PROPERTIES
    LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/threads/lib.version)
if(APPLE)
  set_property(TARGET ${target} APPEND_STRING PROPERTY
      LINK_FLAGS "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/threads/osx.syms")
elseif(WIN32)
# Nothing needed here, we use __declspec(dllexport) (jpegli_threads_export.h)
else()
  set_property(TARGET jpegli_threads APPEND_STRING PROPERTY
      LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/threads/lib.version")
endif()  # APPLE

# Compile the shared library such that the JPEGLI_THREADS_EXPORT symbols are
# exported. Users of the library will not set this flag and therefore import
# those symbols.
target_compile_definitions(jpegli_threads
  PRIVATE -DJPEGLI_THREADS_INTERNAL_LIBRARY_BUILD)

generate_export_header(jpegli_threads
  BASE_NAME JPEGLI_THREADS
  EXPORT_FILE_NAME include/jpegli/jpegli_threads_export.h)
target_compile_definitions(jpegli_threads PUBLIC
  "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:JPEGLI_THREADS_STATIC_DEFINE>")
# Place all public headers in a single directory.
foreach(path ${JPEGLI_INTERNAL_THREADS_PUBLIC_HEADERS})
  configure_file(
    ${path}
    ${path}
    COPYONLY
  )
endforeach()

### Add a pkg-config file for libjpegli_threads.

# Allow adding prefix if CMAKE_INSTALL_INCLUDEDIR not absolute.
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(PKGCONFIG_TARGET_INCLUDES "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(PKGCONFIG_TARGET_INCLUDES "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()
# Allow adding prefix if CMAKE_INSTALL_LIBDIR not absolute.
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(PKGCONFIG_TARGET_LIBS "${CMAKE_INSTALL_LIBDIR}")
else()
    set(PKGCONFIG_TARGET_LIBS "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()

if (BUILD_SHARED_LIBS)
  set(JPEGLI_REQUIRES_TYPE "Requires.private")
else()
  set(JPEGLI_REQUIRES_TYPE "Requires")
endif()

set(JPEGLI_THREADS_LIBRARY_REQUIRES "")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/threads/libjpegli_threads.pc.in"
               "libjpegli_threads.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libjpegli_threads.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
