#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/engine_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_inc.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/print.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_LINUX ${NEO_CORE_OS_INTERFACE_LINUX})
add_subdirectories()
