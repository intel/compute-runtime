#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

set(NEO_SHARED_DLLS
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
)

add_library(neo_shared_dlls_precompiled_objects OBJECT ${NEO_SHARED_DLLS})
target_include_directories(neo_shared_dlls_precompiled_objects PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_RELEASE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(neo_shared_dlls_precompiled_objects PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_RELEASE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)