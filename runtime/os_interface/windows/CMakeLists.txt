#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_OS_INTERFACE_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/api_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d10_11_sharing_functions.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d9_sharing_functions.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d_sharing_functions.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/device_caps_init_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/environment_variables.h
  ${CMAKE_CURRENT_SOURCE_DIR}/ocl_reg_path.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_metrics_library.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_device_command_stream.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_device_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/wddm_memory_manager_allocate_in_device_pool.cpp
)

if(WIN32)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE
    ${RUNTIME_SRCS_OS_INTERFACE_WINDOWS}
  )
endif()

add_subdirectories()
