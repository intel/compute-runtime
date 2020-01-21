#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_OS_INTERFACE_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/allocator_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/api_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d_sharing_functions.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/device_factory_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_allocation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_allocation.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_bdw_plus.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/drm_memory_manager_allocate_in_device_pool.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_operations_handler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_operations_handler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux_inc.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ocl_reg_path.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_metrics_library.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux.h
)
if(UNIX)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_OS_INTERFACE_LINUX})
endif()

set_property(GLOBAL PROPERTY RUNTIME_SRCS_OS_INTERFACE_LINUX ${RUNTIME_SRCS_OS_INTERFACE_LINUX})
add_subdirectories()
