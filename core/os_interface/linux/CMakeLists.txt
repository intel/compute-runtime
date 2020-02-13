#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/allocator_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_allocation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_allocation.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/drm_memory_manager_allocate_in_device_pool.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/drm_engine_mapper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_engine_mapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_null_device.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_operations_handler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_operations_handler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_device_id.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_device_id_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux_inc.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/drm_query.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/engine_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_inc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_socket.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table_manager_functions.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/print.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sys_calls.h
)

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_LINUX ${NEO_CORE_OS_INTERFACE_LINUX})
add_subdirectories()
