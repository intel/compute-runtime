#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_UTILITIES
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/clflush.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_file_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_file_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_reader_creator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/heap_allocator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/heap_allocator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/spinlock.h
)

set(NEO_CORE_UTILITIES_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/cpu_info.cpp
)

set(NEO_CORE_UTILITIES_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/cpu_info.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_UTILITIES ${NEO_CORE_UTILITIES})
set_property(GLOBAL PROPERTY NEO_CORE_UTILITIES_WINDOWS ${NEO_CORE_UTILITIES_WINDOWS})
set_property(GLOBAL PROPERTY NEO_CORE_UTILITIES_LINUX ${NEO_CORE_UTILITIES_LINUX})