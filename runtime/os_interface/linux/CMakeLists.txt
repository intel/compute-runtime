#
# Copyright (C) 2018 Intel Corporation
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
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_32bit_memory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_allocation.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_engine_mapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_engine_mapper.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo_create.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_null_device.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux_inc.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_inc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/print.cpp
)

if(UNIX)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_OS_INTERFACE_LINUX})
endif()
set_property(GLOBAL PROPERTY RUNTIME_SRCS_OS_INTERFACE_LINUX ${RUNTIME_SRCS_OS_INTERFACE_LINUX})
add_subdirectories()
