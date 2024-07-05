# Copyright (c) the JPEG XL Project Authors.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Tools are added conditionally below.
set(TOOL_BINARIES)
# Tools that depend on jxl internal functions.
set(INTERNAL_TOOL_BINARIES)
set(FUZZER_CORPUS_BINARIES)

add_library(jxl_tool STATIC EXCLUDE_FROM_ALL
  cmdline.cc
  speed_stats.cc
  tool_version.cc
  ${JXL_CMS_OBJECTS}
)
target_compile_options(jxl_tool PUBLIC "${JPEGXL_INTERNAL_FLAGS}")
target_include_directories(jxl_tool PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(jxl_tool PUBLIC hwy)

# The JPEGXL_VERSION is set from the builders.
if(NOT DEFINED JPEGXL_VERSION OR JPEGXL_VERSION STREQUAL "")
  find_package(Git QUIET)
  execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
      OUTPUT_VARIABLE GIT_REV
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      ERROR_QUIET)
  string(STRIP "${GIT_REV}" GIT_REV)
  if(GIT_REV STREQUAL "")
    set(JPEGXL_VERSION "(unknown)")
  endif()
endif()

if(NOT DEFINED JPEGXL_VERSION OR JPEGXL_VERSION STREQUAL "")
  # We are building from a git environment and the user didn't set
  # JPEGXL_VERSION. Make a target that computes the GIT_REV at build-time always
  # but only updates the file if it changed. This allows rebuilds without
  # modifying cmake files to update the JPEGXL_VERSION.
  message(STATUS "Building with JPEGXL_VERSION=${GIT_REV} (auto-updated)")
  add_custom_target(
    tool_version_git
    ${CMAKE_COMMAND}
      -D JPEGXL_ROOT_DIR=${CMAKE_SOURCE_DIR}
      -D DST=${CMAKE_CURRENT_BINARY_DIR}/tool_version_git.h
      -P ${CMAKE_CURRENT_SOURCE_DIR}/git_version.cmake
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/tool_version_git.h"
  )
  add_dependencies(jxl_tool tool_version_git)

  set_source_files_properties(tool_version.cc PROPERTIES
    COMPILE_DEFINITIONS JPEGXL_VERSION_FROM_GIT=1)
  target_include_directories(jxl_tool PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
  # Note: Ninja looks for dependencies on the jxl_tool target before running
  # the tool_version_git targets, so when updating the tool_version_git.h the
  # jxl_tool target is not rebuilt. This forces to generate it at configure time
  # if needed.
  execute_process(
    COMMAND ${CMAKE_COMMAND}
      -D JPEGXL_ROOT_DIR=${CMAKE_SOURCE_DIR}
      -D DST=${CMAKE_CURRENT_BINARY_DIR}/tool_version_git.h
      -P ${CMAKE_CURRENT_SOURCE_DIR}/git_version.cmake)
else()
  message(STATUS "Building with JPEGXL_VERSION=${JPEGXL_VERSION}")
  set_source_files_properties(tool_version.cc PROPERTIES
    COMPILE_DEFINITIONS JPEGXL_VERSION=\"${JPEGXL_VERSION}\")
endif()

add_library(jxl_gauss_blur STATIC #EXCLUDE_FROM_ALL
  gauss_blur.cc
)
target_compile_options(jxl_gauss_blur PUBLIC "${JPEGXL_INTERNAL_FLAGS}")
target_include_directories(jxl_gauss_blur PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(jxl_gauss_blur PUBLIC jxl)
target_link_libraries(jxl_gauss_blur PUBLIC hwy)

if(JPEGXL_ENABLE_TOOLS)
  # Depends on parts of jxl_extras that are only built if libjpeg is found and
  # jpegli is enabled.
  add_executable(cjpegli cjpegli.cc)
  add_executable(djpegli djpegli.cc)
  list(APPEND INTERNAL_TOOL_BINARIES cjpegli djpegli)
endif()  # JPEGXL_ENABLE_TOOLS

# Other developer tools.
if(JPEGXL_ENABLE_DEVTOOLS)
  list(APPEND FUZZER_CORPUS_BINARIES jpegli_dec_fuzzer_corpus)
  add_executable(jpegli_dec_fuzzer_corpus jpegli_dec_fuzzer_corpus.cc)
  target_link_libraries(jpegli_dec_fuzzer_corpus
    jpegli-static
    jxl_tool
    jxl_threads
  )
endif()  # JPEGXL_ENABLE_DEVTOOLS

# Benchmark tools.
if(JPEGXL_ENABLE_BENCHMARK AND JPEGXL_ENABLE_TOOLS)
  list(APPEND INTERNAL_TOOL_BINARIES
    benchmark_xl
  )

  add_executable(benchmark_xl
    benchmark/benchmark_xl.cc
    benchmark/benchmark_args.cc
    benchmark/benchmark_codec.cc
    benchmark/benchmark_file_io.cc
    benchmark/benchmark_stats.cc
    benchmark/benchmark_utils.cc
    benchmark/benchmark_utils.h
    benchmark/benchmark_codec_jpeg.cc
    benchmark/benchmark_codec_jpeg.h
    ssimulacra2.cc
    ../third_party/dirent.cc
  )
  target_link_libraries(benchmark_xl Threads::Threads)
  target_link_libraries(benchmark_xl jxl_gauss_blur) # for ssimulacra
  if(MINGW)
  # MINGW doesn't support glob.h.
  target_compile_definitions(benchmark_xl PRIVATE "-DHAS_GLOB=0")
  endif() # MINGW
endif()  # JPEGXL_ENABLE_BENCHMARK

# All tool binaries depend on "jxl" library and the tool helpers.
foreach(BINARY IN LISTS INTERNAL_TOOL_BINARIES)
  target_link_libraries("${BINARY}"
    jxl_extras-internal
    jxl_threads
    jxl_tool
  )
endforeach()

list(APPEND TOOL_BINARIES ${INTERNAL_TOOL_BINARIES} ${FUZZER_CORPUS_BINARIES})

foreach(BINARY IN LISTS TOOL_BINARIES)
  if(EMSCRIPTEN)
    set(JXL_WASM_TOOLS_LINK_FLAGS "\
      -s USE_LIBPNG=1 \
      -s ALLOW_MEMORY_GROWTH=1 \
    ")
    if (JPEGXL_ENABLE_WASM_THREADS)
      set(JXL_WASM_TOOLS_LINK_FLAGS "${JXL_WASM_TOOLS_LINK_FLAGS} \
        -s USE_PTHREADS=1 \
        -s PTHREAD_POOL_SIZE=16 \
      ")
    endif()
    set_target_properties(${BINARY} PROPERTIES LINK_FLAGS "${JXL_WASM_TOOLS_LINK_FLAGS}")
  endif()
endforeach()

install(TARGETS ${TOOL_BINARIES} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
message(STATUS "Building tools: ${TOOL_BINARIES}")

# jpegli_dec_fuzzer builds even when not JPEGXL_ENABLE_TOOLS
set(FUZZER_BINARIES jpegli_dec_fuzzer)

# Fuzzers.
foreach(FUZZER IN LISTS FUZZER_BINARIES)
  if(JPEGXL_ENABLE_FUZZERS)
    set(BINARY "${FUZZER}")
    add_executable("${BINARY}" "${BINARY}.cc")
    target_link_libraries("${BINARY}" ${JPEGXL_FUZZER_LINK_FLAGS})
  else()
    # When not enabled we want a lightweight alternative for regular fuzzers
    # that just run the target.
    set(BINARY "${FUZZER}_runner")
    add_executable("${BINARY}" EXCLUDE_FROM_ALL
        "fuzzer_stub.cc" "${FUZZER}.cc")
  endif()  # JPEGXL_ENABLE_FUZZERS
  target_include_directories("${BINARY}" PRIVATE "${CMAKE_SOURCE_DIR}")
  target_link_libraries("${BINARY}" jpegli-static)
endforeach()

if(JPEGXL_ENABLE_JNI)
find_package(JNI QUIET)
find_package(Java QUIET)

if (JNI_FOUND AND Java_FOUND)
  include(UseJava)

endif()  # JNI_FOUND & Java_FOUND
endif()  # JPEGXL_ENABLE_JNI

# End-to-end tests for the tools
if(JPEGXL_TEST_TOOLS)
find_program (BASH_PROGRAM bash)
if (BASH_PROGRAM)
  set(TEST_SCRIPTS)
  find_package(JPEG)
  if (JPEG_FOUND)
    list(APPEND TEST_SCRIPTS jpegli_tools_test)
  endif()
  foreach(SCRIPT IN LISTS TEST_SCRIPTS)
    add_test(NAME ${SCRIPT}
      COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/${SCRIPT}.sh
      ${CMAKE_BINARY_DIR})
  endforeach()
endif()  # BASH_PROGRAM
endif()  # JPEGXL_TEST_TOOLS
