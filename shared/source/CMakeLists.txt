#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(SHARED_SOURCE_PROJECTS_FOLDER "neo shared")
if(NOT DEFINED NEO_SHARED_RELEASE_LIB_NAME)
  MESSAGE(FATAL_ERROR "NEO Shared library name undefined!")
endif()

include(enable_cores.cmake)

function(generate_shared_lib LIB_NAME MOCKABLE)
  add_library(${LIB_NAME} STATIC
              ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
              ${CMAKE_CURRENT_SOURCE_DIR}/enable_cores.cmake
              ${CORE_SOURCES}
  )
  if(${MOCKABLE})
    target_compile_definitions(${LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=virtual MOCKABLE= DEFAULT_TEST_PLATFORM=${DEFAULT_TESTED_PLATFORM} ${TESTED_CORE_FLAGS_DEFINITONS})
  else()
    target_compile_definitions(${LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
  endif()

  list(APPEND LIB_FLAGS_DEFINITIONS ${SUPPORTED_CORE_FLAGS_DEFINITONS})
  list(APPEND LIB_FLAGS_DEFINITIONS ${AOT_DEFINITIONS})

  set_property(TARGET ${LIB_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
  set_target_properties(${LIB_NAME} PROPERTIES FOLDER "${SHARED_SOURCE_PROJECTS_FOLDER}")
  target_compile_definitions(${LIB_NAME} PUBLIC PUBLIC GMM_LIB_DLL ${LIB_FLAGS_DEFINITIONS} DEFAULT_PLATFORM=${DEFAULT_SUPPORTED_PLATFORM} PRIVATE OGL=1)
  target_compile_definitions(${LIB_NAME} PUBLIC ${NEO_CORE_COMPILE_DEFS} ${NEO__IGC_COMPILE_DEFINITIONS})

  target_include_directories(${LIB_NAME} PUBLIC
                             ${AOT_CONFIG_HEADERS_DIR}
                             ${ENGINE_NODE_DIR}
                             ${NEO__GMM_INCLUDE_DIR}
                             ${CIF_BASE_DIR}
                             ${IGC_OCL_ADAPTOR_DIR}
                             ${NEO__IGC_INCLUDE_DIR}
                             ${KHRONOS_HEADERS_DIR}
                             ${VISA_INCLUDE_DIR}
                             ${CMAKE_CURRENT_SOURCE_DIR}/os_interface/create_command_stream_receiver_${DRIVER_MODEL}
                             ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission/create_direct_submission_${DRIVER_MODEL}
  )

  if(WIN32 OR NOT DISABLE_WDDM_LINUX)
    target_include_directories(${LIB_NAME} PUBLIC ${WDK_INCLUDE_PATHS})
  endif()

  if(WIN32)
    target_include_directories(${LIB_NAME} PUBLIC
                               ${CMAKE_CURRENT_SOURCE_DIR}/os_interface/windows
    )
  else()
    target_include_directories(${LIB_NAME} PUBLIC
                               ${CMAKE_CURRENT_SOURCE_DIR}/os_interface/linux
    )

  endif()

  create_project_source_tree(${LIB_NAME})

  # Enable SSE4/AVX2 options for files that need them
  if(MSVC)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/helpers/${NEO_TARGET_PROCESSOR}/local_id_gen_avx2.cpp PROPERTIES COMPILE_FLAGS /arch:AVX2)
  else()
    if(COMPILER_SUPPORTS_AVX2)
      set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/helpers/${NEO_TARGET_PROCESSOR}/local_id_gen_avx2.cpp PROPERTIES COMPILE_FLAGS -mavx2)
    endif()
    if(COMPILER_SUPPORTS_SSE42)
      set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/helpers/local_id_gen_sse4.cpp PROPERTIES COMPILE_FLAGS -msse4.2)
    endif()
  endif()

endfunction()

set(NEO_CORE_COMPILE_DEFS "")
set(CORE_SOURCES ${CORE_SRCS_COREX_ALL_BASE})

add_subdirectories()

if(WIN32)
  list(APPEND CORE_SOURCES ${CORE_SRCS_COREX_ALL_WINDOWS})
endif()

if(WIN32 OR NOT DISABLE_WDDM_LINUX)
  list(APPEND CORE_SOURCES ${CORE_SRCS_COREX_ALL_WDDM})
endif()

if(NOT WIN32)
  list(APPEND CORE_SOURCES ${CORE_SRCS_COREX_ALL_LINUX})
endif()

macro(macro_for_each_core_type)
  append_sources_from_properties(CORE_SOURCES
                                 SHARED_SRCS_ADDITIONAL_FILES_${CORE_TYPE}
  )
endmacro()

apply_macro_for_each_core_type("SUPPORTED")

append_sources_from_properties(CORE_SOURCES
                               NEO_CORE_AIL
                               NEO_CORE_ASSERT_HANDLER
                               NEO_CORE_AUB
                               NEO_CORE_AUB_MEM_DUMP
                               NEO_CORE_BUILT_INS
                               NEO_CORE_COMMANDS
                               NEO_CORE_COMMAND_CONTAINER
                               NEO_CORE_COMMAND_ENCODERS
                               NEO_CORE_COMMAND_STREAM
                               NEO_CORE_COMPILER_INTERFACE
                               NEO_CORE_DEBUGGER
                               NEO_CORE_DEBUG_SETTINGS
                               NEO_CORE_DEVICE
                               NEO_CORE_DIRECT_SUBMISSION
                               NEO_CORE_DIRECT_SUBMISSION_DISPATCHERS
                               NEO_CORE_EXECUTION_ENVIRONMENT
                               NEO_CORE_GEN_COMMON
                               NEO_CORE_GMM_HELPER
                               NEO_CORE_HELPERS
                               NEO_CORE_HELPERS_SIP_EXTERNAL_LIB
                               NEO_CORE_IMAGE
                               NEO_CORE_INDIRECT_HEAP
                               NEO_CORE_KERNEL
                               NEO_CORE_MEMORY_MANAGER
                               NEO_CORE_MEMORY_PROPERTIES
                               NEO_CORE_OS_INTERFACE
                               NEO_CORE_PAGE_FAULT_MANAGER
                               NEO_CORE_PROGRAM
                               NEO_CORE_SEMAPHORE
                               NEO_CORE_SKU_INFO_BASE
                               NEO_CORE_SRCS_BUILT_INS
                               NEO_CORE_SRCS_BUILT_INS_OPS
                               NEO_CORE_SRCS_BUILT_IN_KERNELS
                               NEO_CORE_TBX
                               NEO_CORE_UTILITIES
                               NEO_CORE_ZEINFO_EXTRA
                               NEO_DEVICE_BINARY_FORMAT
                               NEO_UNIFIED_MEMORY
                               NEO_CORE_RELEASE_HELPER
                               NEO_GTPIN

)

if(WIN32)
  append_sources_from_properties(CORE_SOURCES
                                 NEO_CORE_COMPILER_INTERFACE_WINDOWS
                                 NEO_CORE_GMM_HELPER_WINDOWS
                                 NEO_CORE_HELPERS_GMM_CALLBACKS_WINDOWS
                                 NEO_CORE_DIRECT_SUBMISSION_WINDOWS
                                 NEO_CORE_OS_INTERFACE_WINDOWS
                                 NEO_CORE_OS_INTERFACE_WDDM
                                 NEO_CORE_PAGE_FAULT_MANAGER_WINDOWS
                                 NEO_CORE_SKU_INFO_WINDOWS
                                 NEO_CORE_SRCS_DEBUGGER_WINDOWS
                                 NEO_CORE_SRCS_HELPERS_WINDOWS
                                 NEO_CORE_UTILITIES_WINDOWS
                                 NEO_CORE_EXECUTION_ENVIRONMENT_WDDM
                                 NEO_CORE_AIL_WINDOWS
  )
else()
  append_sources_from_properties(CORE_SOURCES
                                 NEO_CORE_COMPILER_INTERFACE_LINUX
                                 NEO_CORE_DIRECT_SUBMISSION_LINUX
                                 NEO_CORE_OS_INTERFACE_LINUX
                                 NEO_CORE_PAGE_FAULT_MANAGER_LINUX
                                 NEO_CORE_SRCS_DEBUGGER_LINUX
                                 NEO_CORE_UTILITIES_LINUX
                                 NEO_CORE_EXECUTION_ENVIRONMENT_DRM
                                 NEO_CORE_AIL_LINUX
  )
  if(NOT DISABLE_WDDM_LINUX)
    append_sources_from_properties(CORE_SOURCES
                                   NEO_CORE_HELPERS_GMM_CALLBACKS_WINDOWS
                                   NEO_CORE_SRCS_HELPERS_WINDOWS
                                   NEO_CORE_GMM_HELPER_WINDOWS
                                   NEO_CORE_OS_INTERFACE_WDDM
                                   NEO_CORE_EXECUTION_ENVIRONMENT_WDDM
                                   NEO_CORE_SKU_INFO_WINDOWS
    )
  endif()
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_TYPE}/core_sources.cmake)
  include(${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_TYPE}/core_sources.cmake)
endif()

if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
endif()

if(NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

if(DONT_CARE_OF_VIRTUALS)
  generate_shared_lib(${NEO_SHARED_RELEASE_LIB_NAME} TRUE)
else()
  generate_shared_lib(${NEO_SHARED_RELEASE_LIB_NAME} FALSE)
  if(NOT NEO_SKIP_UNIT_TESTS)
    generate_shared_lib(${NEO_SHARED_MOCKABLE_LIB_NAME} TRUE)
  endif()
endif()

set_property(GLOBAL APPEND PROPERTY NEO_CORE_SRCS_LINK ${CORE_SRCS_LINK})

get_property(NEO_SRCS_ENABLE_CORE GLOBAL PROPERTY NEO_SRCS_ENABLE_CORE)
target_sources(ocloc_lib PRIVATE ${NEO_SRCS_ENABLE_CORE})
if(UNIX)
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_SRCS_LINK ${CORE_SRCS_LINK_LINUX})
  target_include_directories(${NEO_SHARED_RELEASE_LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pin/linux)
  if(NOT NEO_SKIP_UNIT_TESTS)
    target_include_directories(${NEO_SHARED_MOCKABLE_LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pin/linux)
  endif()
else()
  target_include_directories(${NEO_SHARED_RELEASE_LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pin/windows)
  if(NOT NEO_SKIP_UNIT_TESTS)
    target_include_directories(${NEO_SHARED_MOCKABLE_LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pin/windows)
  endif()
endif()
