#
# Copyright (C) 2019-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/base_object_utils.h
               ${CMAKE_CURRENT_SOURCE_DIR}/const_stringref_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/containers_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/containers_tests_helpers.h
               ${CMAKE_CURRENT_SOURCE_DIR}/cpuintrinsics_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/destructor_counted.h
               ${CMAKE_CURRENT_SOURCE_DIR}/heap_allocator_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/io_functions_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/numeric_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/perf_profiler_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/reference_tracked_object_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/software_tags_manager_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/spinlock_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/timer_util_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/vec_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/wait_util_tests.cpp
)

add_subdirectories()
