# Copyright (c) 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif()

if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW)
endif()

project(neo)
if(NOT TARGET ${BIKSIM_LIB_NAME})
  add_subdirectory(builtin_kernels_simulation)
endif()

hide_subdir(builtin_kernels_simulation)
hide_subdir(dll)
hide_subdir(instrumentation)

add_library(${NEO_STATIC_LIB_NAME} STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/enable_gens.cmake
  $<TARGET_OBJECTS:${BIKSIM_LIB_NAME}>
)

add_subdirectories()
add_subdirectory(instrumentation${IGDRCL__INSTRUMENTATION_DIR_SUFFIX})

if(WIN32)
  if("${IGDRCL_OPTION__BITS}" STREQUAL "32" )
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
  endif()
endif()

include(enable_gens.cmake)

# Enable SSE4/AVX2 options for files that need them
if(MSVC)
  set_source_files_properties(command_queue/local_id_gen_avx2.cpp PROPERTIES COMPILE_FLAGS /arch:AVX2)
else()
  set_source_files_properties(command_queue/local_id_gen_avx2.cpp PROPERTIES COMPILE_FLAGS -mavx2)
  set_source_files_properties(command_queue/local_id_gen_sse4.cpp PROPERTIES COMPILE_FLAGS -msse4.2)
endif()

# Put Driver version into define
if(NEO_DRIVER_VERSION)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/device/device_caps.cpp PROPERTIES COMPILE_DEFINITIONS NEO_DRIVER_VERSION="${NEO_DRIVER_VERSION}")
endif()

target_link_libraries(${NEO_STATIC_LIB_NAME} elflib)

target_include_directories(${NEO_STATIC_LIB_NAME} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${GMM_INCLUDE_PATHS}
  ${KMDAF_HEADERS_DIR}
  ${UMKM_SHAREDDATA_INCLUDE_PATHS}
)

target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
  ${CIF_BASE_DIR}
  ${GTPIN_HEADERS_DIR}
  ${IGC_OCL_ADAPTOR_DIR}
  ${IGDRCL__IGC_INCLUDE_DIR}
  ${KHRONOS_HEADERS_DIR}
  ${THIRD_PARTY_DIR}
)

if(WIN32)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
    ${WDK_INCLUDE_PATHS}
    os_interface/windows
  )
  target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC OGL=1)
  target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC INSTR_WIN_UMD=1)
else()
  target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC OGL_GEM=1)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
    os_interface/linux
    "${LIBDRM_DIR}/include"
  )
endif()

target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC DEFAULT_PLATFORM=${DEFAULT_SUPPORTED_PLATFORM})

link_directories(${GMM_LIB_PATHS})

target_link_libraries(${NEO_STATIC_LIB_NAME} ${GMMUMD_LIB_NAME})

if(INSTRUMENTATION_LIB_NAME)
	add_dependencies(${NEO_STATIC_LIB_NAME} ${INSTRUMENTATION_LIB_NAME})
endif()

if(WIN32)
  if(GTPIN_HEADERS_DIR)
    set( DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dll/windows/GTPinExports${IGDRCL_OPTION__BITS}.def" )
  else()
    set( DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dll/windows/OpenCLExports${IGDRCL_OPTION__BITS}.def" )
  endif()
endif(WIN32)

list(APPEND LIB_FLAGS_DEFINITIONS -DCIF_HEADERS_ONLY_BUILD ${SUPPORTED_GEN_FLAGS_DEFINITONS})

target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC ${LIB_FLAGS_DEFINITIONS})

set(IGDRCL_LIB_FLAGS_DEFINITIONS ${LIB_FLAGS_DEFINITIONS} PARENT_SCOPE)
set_target_properties(${NEO_STATIC_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

set_property(TARGET ${NEO_STATIC_LIB_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})

set_target_properties(${NEO_STATIC_LIB_NAME} PROPERTIES FOLDER "opencl runtime")

target_include_directories(${NEO_STATIC_LIB_NAME} BEFORE PRIVATE ${HW_SRC_INCLUDE_PATH})

if(${GENERATE_EXECUTABLE})
  if(GTPIN_HEADERS_DIR)
    foreach(GEN_NUM RANGE ${MAX_GEN})
      GEN_CONTAINS_PLATFORMS("SUPPORTED" ${GEN_NUM} GENX_HAS_PLATFORMS)
      if(${GENX_HAS_PLATFORMS})
        target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gen${GEN_NUM}/gtpin_setup_gen${GEN_NUM}.cpp)
      endif(${GENX_HAS_PLATFORMS})
    endforeach()
  endif()

  list(APPEND NEO_DYNAMIC_LIB__TARGET_OBJECTS
    $<TARGET_OBJECTS:${SHARINGS_ENABLE_LIB_NAME}>
    $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
    $<TARGET_OBJECTS:${BUILTINS_BINARIES_LIB_NAME}>
    $<TARGET_OBJECTS:${SCHEDULER_BINARY_LIB_NAME}>
  )
  add_library(${NEO_DYNAMIC_LIB_NAME} SHARED
    ${NEO_DYNAMIC_LIB__TARGET_OBJECTS}
  )

  add_subdirectory(dll)

  if(HAVE_INSTRUMENTATION)
    target_link_libraries(${NEO_DYNAMIC_LIB_NAME} instrumentation_umd)
  endif()

  target_link_libraries(${NEO_DYNAMIC_LIB_NAME} ${NEO_STATIC_LIB_NAME})

  target_include_directories(${NEO_DYNAMIC_LIB_NAME} BEFORE PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${HW_SRC_INCLUDE_PATH}
  )

  target_include_directories(${NEO_DYNAMIC_LIB_NAME} PUBLIC
    ${GMM_INCLUDE_PATHS}
    ${UMKM_SHAREDDATA_INCLUDE_PATHS}
    ${INSTRUMENTATION_INCLUDE_PATH}
  )
  if(WIN32)
    target_include_directories(${NEO_DYNAMIC_LIB_NAME} PUBLIC
      ${WDK_INCLUDE_PATHS}
    )
    target_link_libraries(${NEO_DYNAMIC_LIB_NAME} ${NEO_STATIC_LIB_NAME} dxgi Ws2_32.lib)
  else()
    target_include_directories(${NEO_DYNAMIC_LIB_NAME} PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/dll/linux/devices${BRANCH_DIR_SUFFIX}
    )
    target_link_libraries(${NEO_DYNAMIC_LIB_NAME} dl pthread)
    set_property(TARGET ${NEO_DYNAMIC_LIB_NAME}
      APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/dll/linux/ocl.exports"
    )
    set_property(TARGET ${NEO_DYNAMIC_LIB_NAME}
      APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bsymbolic"
    )
    set_property(TARGET ${NEO_DYNAMIC_LIB_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
  endif()

  set_target_properties(${NEO_DYNAMIC_LIB_NAME} PROPERTIES
    DEBUG_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
    RELEASE_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
    RELEASEINTERNAL_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
    OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
  )
  set_target_properties(${NEO_DYNAMIC_LIB_NAME} PROPERTIES FOLDER "opencl runtime")
endif(${GENERATE_EXECUTABLE})

if(WIN32)
  create_project_source_tree(${NEO_STATIC_LIB_NAME})
endif()

if(UNIX AND NOT (TARGET clang-tidy))
  add_custom_target(clang-tidy
    DEPENDS scheduler
  )
  add_custom_command(
    TARGET clang-tidy
    POST_BUILD
    COMMAND echo clang-tidy...
    COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -name *.cpp -print0 | xargs -0 -I{} -P`nproc` clang-tidy -p ${IGDRCL_BINARY_DIR} {} | tee ${IGDRCL_BINARY_DIR}/clang-tidy.log
    WORKING_DIRECTORY ${IGDRCL_SOURCE_DIR}
  )
endif()
