#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_UTILITIES_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/base_object_utils.h
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/const_stringref_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/containers_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/containers_tests_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cpuinfo_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cpuintrinsics_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/destructor_counted.h
  ${CMAKE_CURRENT_SOURCE_DIR}/directory_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/heap_allocator_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/numeric_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/perf_profiler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/reference_tracked_object_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/spinlock_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/timer_util_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/vec_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_UTILITIES_TESTS ${NEO_CORE_UTILITIES_TESTS})