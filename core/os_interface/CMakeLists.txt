#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_memory_operations_handler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_memory_operations_handler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_memory.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_thread.h
  ${CMAKE_CURRENT_SOURCE_DIR}/print.h
)

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE ${NEO_CORE_OS_INTERFACE})
add_subdirectories()
