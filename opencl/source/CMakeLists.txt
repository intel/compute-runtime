#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif()

if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW)
endif()

project(neo)
set(MSVC_DEF_ADDITIONAL_EXPORTS "")

if(NOT TARGET ${BIKSIM_LIB_NAME})
  add_subdirectory(builtin_kernels_simulation)
endif()

hide_subdir(builtin_kernels_simulation)
hide_subdir(dll)
hide_subdir(instrumentation)

add_library(${NEO_STATIC_LIB_NAME} STATIC EXCLUDE_FROM_ALL
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/enable_gens.cmake
  $<TARGET_OBJECTS:${BIKSIM_LIB_NAME}>
)

add_subdirectories()
add_subdirectory(instrumentation${NEO__INSTRUMENTATION_DIR_SUFFIX})
include(enable_gens.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/core_files.cmake)

# Enable SSE4/AVX2 options for files that need them
if(MSVC)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/command_queue/local_id_gen_avx2.cpp PROPERTIES COMPILE_FLAGS /arch:AVX2)
else()
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/command_queue/local_id_gen_avx2.cpp PROPERTIES COMPILE_FLAGS -mavx2)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/command_queue/local_id_gen_sse4.cpp PROPERTIES COMPILE_FLAGS -msse4.2)
endif()

if(WIN32)
  if("${IGDRCL_OPTION__BITS}" STREQUAL "32" )
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
  endif()
endif()

if(DEFINED AUB_STREAM_DIR)
  target_link_libraries(${NEO_STATIC_LIB_NAME} ${AUB_STREAM_PROJECT_NAME})
endif()

target_include_directories(${NEO_STATIC_LIB_NAME} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${KMDAF_HEADERS_DIR}
)

target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
  ${ENGINE_NODE_DIR}
  ${KHRONOS_HEADERS_DIR}
  ${KHRONOS_GL_HEADERS_DIR}
  ${CIF_BASE_DIR}
  ${IGC_OCL_ADAPTOR_DIR}
  ${VISA_INCLUDE_DIR}
  ${NEO__IGC_INCLUDE_DIR}
  ${THIRD_PARTY_DIR}
  ${NEO__GMM_INCLUDE_DIR}
  ${SOURCE_LEVEL_DEBUGGER_HEADERS_DIR}
)

target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC
  ""
  ${NEO__IGC_COMPILE_DEFINITIONS}
)

if(WIN32)
  set(IGDRCL_EXTRA_LIBS Ws2_32)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
    ${WDK_INCLUDE_PATHS}
    ${NEO_CORE_DIRECTORY}/os_interface/windows
    ${NEO_SOURCE_DIR}/opencl/source/os_interface/windows
  )
  target_compile_definitions(${NEO_STATIC_LIB_NAME} PRIVATE OGL=1)
else()
  set(IGDRCL_EXTRA_LIBS dl pthread)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
    ${NEO_CORE_DIRECTORY}/os_interface/linux
    ${NEO_SOURCE_DIR}/opencl/source/os_interface/linux
    ${I915_INCLUDES_DIR}
  )
endif()

target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC GMM_LIB_DLL DEFAULT_PLATFORM=${DEFAULT_SUPPORTED_PLATFORM})

list(APPEND LIB_FLAGS_DEFINITIONS -DCIF_HEADERS_ONLY_BUILD ${SUPPORTED_GEN_FLAGS_DEFINITONS})

target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC ${LIB_FLAGS_DEFINITIONS})

set_target_properties(${NEO_STATIC_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

set_property(TARGET ${NEO_STATIC_LIB_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})

set_target_properties(${NEO_STATIC_LIB_NAME} PROPERTIES FOLDER "opencl runtime")

set(EXPORTS_FILENAME "")
if(WIN32)
    set(EXPORTS_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/OpenCLExports${IGDRCL_OPTION__BITS}.def")
    set(MSVC_DEF_LIB_NAME "igdrcl${IGDRCL_OPTION__BITS}")
    set(MSVC_DEF_HEADER "This file was generated during CMake project configuration - please don't edit")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/dll/windows/OpenCLExports.def.in" "${EXPORTS_FILENAME}")
endif(WIN32)

if(${GENERATE_EXECUTABLE})
  list(APPEND NEO_DYNAMIC_LIB__TARGET_OBJECTS
    $<TARGET_OBJECTS:${SHARINGS_ENABLE_LIB_NAME}>
    $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
    $<TARGET_OBJECTS:${BUILTINS_VME_LIB_NAME}>
    $<TARGET_OBJECTS:${BUILTINS_BINARIES_LIB_NAME}>
    $<TARGET_OBJECTS:${SCHEDULER_BINARY_LIB_NAME}>
  )

  if(DEFINED AUB_STREAM_DIR)
    list(APPEND NEO_DYNAMIC_LIB__TARGET_OBJECTS $<TARGET_OBJECTS:${AUB_STREAM_PROJECT_NAME}_all_hw>)
  endif()

  add_library(${NEO_DYNAMIC_LIB_NAME} SHARED
    ${NEO_DYNAMIC_LIB__TARGET_OBJECTS}
    ${NEO_SOURCE_DIR}/opencl/source/aub/aub_stream_interface.cpp
  )

  if(UNIX)
    install(FILES
      $<TARGET_FILE:${NEO_DYNAMIC_LIB_NAME}>
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/intel-opencl
      COMPONENT opencl
    )
    set_property(GLOBAL APPEND PROPERTY NEO_COMPONENTS_LIST "opencl")
  endif()

  if(NOT DISABLED_GTPIN_SUPPORT)
    macro(macro_for_each_gen)
      target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${GEN_TYPE_LOWER}/gtpin_setup_${GEN_TYPE_LOWER}.cpp)
    endmacro()
    apply_macro_for_each_gen("SUPPORTED")
  endif()

  add_subdirectory(dll)

  target_link_libraries(${NEO_DYNAMIC_LIB_NAME} ${NEO_STATIC_LIB_NAME} ${NEO_CORE_LIB} ${NEO_STATIC_LIB_NAME} ${IGDRCL_EXTRA_LIBS})

  target_include_directories(${NEO_DYNAMIC_LIB_NAME} BEFORE PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${AUB_STREAM_DIR}/..
  )

  if(WIN32)
    target_link_libraries(${NEO_DYNAMIC_LIB_NAME} dxgi)
    add_dependencies(${NEO_DYNAMIC_LIB_NAME} ${GMM_TARGET_NAME})
    target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE
      ${NEO_CORE_DIRECTORY}/os_interface/windows/gmm_interface_win.cpp
    )
  else()
    target_link_libraries(${NEO_DYNAMIC_LIB_NAME} ${GMM_LINK_NAME})
    target_include_directories(${NEO_DYNAMIC_LIB_NAME} PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/dll/linux/devices${BRANCH_DIR_SUFFIX}
    )
    set_property(TARGET ${NEO_DYNAMIC_LIB_NAME}
      APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/dll/linux/ocl.exports"
    )
    set_property(TARGET ${NEO_DYNAMIC_LIB_NAME}
      APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bsymbolic"
    )
    set_property(TARGET ${NEO_DYNAMIC_LIB_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
    target_sources(${NEO_DYNAMIC_LIB_NAME} PRIVATE
      ${NEO_CORE_DIRECTORY}/os_interface/linux/gmm_interface_linux.cpp
    )
  endif()

  set_target_properties(${NEO_DYNAMIC_LIB_NAME} PROPERTIES
    DEBUG_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
    RELEASE_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
    RELEASEINTERNAL_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
    OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
  )
  set_target_properties(${NEO_DYNAMIC_LIB_NAME} PROPERTIES FOLDER "opencl runtime")
  create_project_source_tree_with_exports(${NEO_DYNAMIC_LIB_NAME} "${EXPORTS_FILENAME}")
endif(${GENERATE_EXECUTABLE})

create_project_source_tree(${NEO_STATIC_LIB_NAME})

if(UNIX AND NOT (TARGET clang-tidy))
  add_custom_target(clang-tidy
    DEPENDS scheduler
  )
  add_custom_command(
    TARGET clang-tidy
    POST_BUILD
    COMMAND echo clang-tidy...
    COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -name *.cpp | xargs --verbose -I{} -P`nproc` clang-tidy-8 -p ${NEO_BINARY_DIR} {} | tee ${NEO_BINARY_DIR}/clang-tidy.log
    WORKING_DIRECTORY ${NEO_SOURCE_DIR}
  )
endif()

set(IGDRCL_EXTRA_LIBS ${IGDRCL_EXTRA_LIBS} PARENT_SCOPE)
