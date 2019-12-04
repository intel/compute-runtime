#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_registry_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_registry_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/print.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows_wrapper.h
)

if(WIN32)
  set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_WINDOWS ${NEO_CORE_OS_INTERFACE_WINDOWS})
endif()