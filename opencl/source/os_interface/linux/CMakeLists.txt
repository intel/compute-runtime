#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_OS_INTERFACE_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/api_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d_sharing_functions.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_caps_init_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_bdw_plus.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/ocl_reg_path.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_metrics_library.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux.h
)
if(UNIX)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_OS_INTERFACE_LINUX})
endif()

set_property(GLOBAL PROPERTY RUNTIME_SRCS_OS_INTERFACE_LINUX ${RUNTIME_SRCS_OS_INTERFACE_LINUX})
add_subdirectories()
