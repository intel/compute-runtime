#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/drm_engine_mapper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_engine_mapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo_create.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_null_device.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/drm_query.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/engine_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_inc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table_manager_functions.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/print.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_LINUX ${NEO_CORE_OS_INTERFACE_LINUX})
add_subdirectories()
