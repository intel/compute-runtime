#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_utilities
  ${CMAKE_CURRENT_SOURCE_DIR}/base_object_utils.h
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/containers_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/containers_tests_helpers
  ${CMAKE_CURRENT_SOURCE_DIR}/cpuinfo_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/debug_file_reader_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_file_reader_tests.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_reader_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/destructor_counted.h
  ${CMAKE_CURRENT_SOURCE_DIR}/directory_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/heap_allocator_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/numeric_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/perf_profiler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/reference_tracked_object_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/spinlock_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tag_allocator_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/timer_util_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/vec_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_utilities})
