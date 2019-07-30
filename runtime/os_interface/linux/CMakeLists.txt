#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_OS_INTERFACE_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/allocator_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/api_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/d3d_sharing_functions.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/device_factory_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_allocation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_allocation.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/drm_engine_mapper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_engine_mapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/drm_memory_manager_allocate_in_device_pool.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo_create.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_null_device.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/drm_query.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_residency_handler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_residency_handler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/engine_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux_inc.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_inc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_metrics_library.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/print.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tiling_mode_helper.h
)

if(UNIX)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_OS_INTERFACE_LINUX})
endif()
set_property(GLOBAL PROPERTY RUNTIME_SRCS_OS_INTERFACE_LINUX ${RUNTIME_SRCS_OS_INTERFACE_LINUX})
add_subdirectories()
