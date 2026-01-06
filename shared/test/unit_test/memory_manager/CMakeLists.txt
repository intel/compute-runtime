#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/alignment_selector_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_allocation_deletion_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/gfx_partition_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/graphics_allocation_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_manager_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/internal_allocation_storage_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/local_memory_usage_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_allocate_in_device_pool_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_allocate_in_preferred_pool_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_multi_device_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/memory_pool_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/multi_graphics_allocation_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/page_table_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/physical_address_allocator_hw_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/physical_address_allocator_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/prefetch_manager_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/special_heap_pool_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/storage_info_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/surface_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_manager_cache_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_manager_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_pooling_tests.cpp
)

add_subdirectories()