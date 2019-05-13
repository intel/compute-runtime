#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_UTILITIES_BASE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/api_intercept.h
  ${CMAKE_CURRENT_SOURCE_DIR}/arrayref.h
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
  ${CMAKE_CURRENT_SOURCE_DIR}/timer_util.h
)

set(RUNTIME_SRCS_UTILITIES_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/directory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/timer_util.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/cpu_info.cpp
)

set(RUNTIME_SRCS_UTILITIES_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/directory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/timer_util.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/cpu_info.cpp
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_UTILITIES_BASE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_UTILITIES_BASE ${RUNTIME_SRCS_UTILITIES_BASE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_UTILITIES_LINUX ${RUNTIME_SRCS_UTILITIES_LINUX})
if(WIN32)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_UTILITIES_WINDOWS})
else()
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_UTILITIES_LINUX})
endif()
