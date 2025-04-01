#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}debug_file_reader_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/buffer_pool_allocator_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/const_stringref_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/containers_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/containers_tests_helpers.h
               ${CMAKE_CURRENT_SOURCE_DIR}/cpuintrinsics_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_file_reader_tests.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_reader_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/directory_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/heap_allocator_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/io_functions_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/logger_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/numeric_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/perf_profiler_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/reference_tracked_object_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/software_tags_manager_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sorted_vector_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/spinlock_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/tag_allocator_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/timer_util_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_pool_allocator_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/vec_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/wait_util_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/isa_pool_allocator_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/staging_buffer_manager_tests.cpp
)

add_subdirectories()