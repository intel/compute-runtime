#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_UTILITIES
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/api_intercept.h
  ${CMAKE_CURRENT_SOURCE_DIR}/arrayref.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cpuintrinsics.h
  ${CMAKE_CURRENT_SOURCE_DIR}/const_stringref.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_file_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_file_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_reader_creator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_reader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/directory.h
  ${CMAKE_CURRENT_SOURCE_DIR}/heap_allocator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/heap_allocator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/iflist.h
  ${CMAKE_CURRENT_SOURCE_DIR}/idlist.h
  ${CMAKE_CURRENT_SOURCE_DIR}/numeric.h
  ${CMAKE_CURRENT_SOURCE_DIR}/perf_profiler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/perf_profiler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/range.h
  ${CMAKE_CURRENT_SOURCE_DIR}/reference_tracked_object.h
  ${CMAKE_CURRENT_SOURCE_DIR}/spinlock.h
  ${CMAKE_CURRENT_SOURCE_DIR}/stackvec.h
  ${CMAKE_CURRENT_SOURCE_DIR}/tag_allocator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/time_measure_wrapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/timer_util.h
)

set(NEO_CORE_UTILITIES_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/cpu_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/directory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/timer_util.cpp
)

set(NEO_CORE_UTILITIES_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/cpu_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/directory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/timer_util.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_UTILITIES ${NEO_CORE_UTILITIES})
set_property(GLOBAL PROPERTY NEO_CORE_UTILITIES_WINDOWS ${NEO_CORE_UTILITIES_WINDOWS})
set_property(GLOBAL PROPERTY NEO_CORE_UTILITIES_LINUX ${NEO_CORE_UTILITIES_LINUX})