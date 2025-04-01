#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_UTILITIES
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/api_intercept.h
    ${CMAKE_CURRENT_SOURCE_DIR}/arrayref.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bitcontainers.h
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
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_timestamps.h
    ${CMAKE_CURRENT_SOURCE_DIR}/iflist.h
    ${CMAKE_CURRENT_SOURCE_DIR}/idlist.h
    ${CMAKE_CURRENT_SOURCE_DIR}/io_functions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/logger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/logger_neo_only.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lookup_array.h
    ${CMAKE_CURRENT_SOURCE_DIR}/metrics_library.h
    ${CMAKE_CURRENT_SOURCE_DIR}/numeric.h
    ${CMAKE_CURRENT_SOURCE_DIR}/perf_counter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/perf_profiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/perf_profiler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/range.h
    ${CMAKE_CURRENT_SOURCE_DIR}/reference_tracked_object.h
    ${CMAKE_CURRENT_SOURCE_DIR}/shared_pool_allocation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/software_tags.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/software_tags.h
    ${CMAKE_CURRENT_SOURCE_DIR}/software_tags_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/software_tags_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sorted_vector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/spinlock.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stackvec.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tag_allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tag_allocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tag_allocator.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/time_measure_wrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/timer_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_pool_allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/timestamp_pool_allocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wait_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wait_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/isa_pool_allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/isa_pool_allocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/staging_buffer_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/staging_buffer_manager.h
)

set(NEO_CORE_UTILITIES_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/cpu_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/directory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/timer_util.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_UTILITIES ${NEO_CORE_UTILITIES})
set_property(GLOBAL PROPERTY NEO_CORE_UTILITIES_WINDOWS ${NEO_CORE_UTILITIES_WINDOWS})

add_subdirectories()
