#
# Copyright (C) 2025-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

set(NEO_SHARED_DLLS
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_options_parser_dll.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/create_deferred_deleter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/create_memory_manager_${DRIVER_MODEL}.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/create_tbx_sockets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_dll.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_controller_enabled.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/get_devices.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_reuse_cleaner_enabled.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/create_command_stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/options_dll.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/get_staging_buffer_size.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/usm_pool_size.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/default_heap_size.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pool_allocator_sizes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pool_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/buffer_pool_size.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/async_launch_mode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_detection.cpp
)

add_library(neo_shared_dlls_precompiled_objects OBJECT ${NEO_SHARED_DLLS})
target_include_directories(neo_shared_dlls_precompiled_objects PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_RELEASE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(neo_shared_dlls_precompiled_objects PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_RELEASE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
if(${NEO_TARGET_PROCESSOR} STREQUAL "x86_64")
  set(NEO_STREAM_COPY_DIR ${NEO_SHARED_DIRECTORY}/helpers/x86_64)
  target_sources(neo_shared_dlls_precompiled_objects PRIVATE
                 ${NEO_STREAM_COPY_DIR}/stream_copy_sse.cpp
                 ${NEO_STREAM_COPY_DIR}/stream_copy_avx2.cpp
                 ${NEO_STREAM_COPY_DIR}/stream_copy_avx512.cpp
  )
  if(MSVC)
    set_source_files_properties(${NEO_STREAM_COPY_DIR}/stream_copy_avx2.cpp PROPERTIES COMPILE_FLAGS /arch:AVX2)
    set_source_files_properties(${NEO_STREAM_COPY_DIR}/stream_copy_avx512.cpp PROPERTIES COMPILE_FLAGS /arch:AVX512)
  else()
    set_source_files_properties(${NEO_STREAM_COPY_DIR}/stream_copy_sse.cpp PROPERTIES COMPILE_FLAGS -msse4.2)
    set_source_files_properties(${NEO_STREAM_COPY_DIR}/stream_copy_avx2.cpp PROPERTIES COMPILE_FLAGS -mavx2)
    set_source_files_properties(${NEO_STREAM_COPY_DIR}/stream_copy_avx512.cpp PROPERTIES COMPILE_FLAGS -mavx512f)
  endif()
endif()

add_subdirectories()