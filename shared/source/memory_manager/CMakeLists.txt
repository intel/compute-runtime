#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_MEMORY_MANAGER
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/allocations_list.h
    ${CMAKE_CURRENT_SOURCE_DIR}/allocations_list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/allocation_type.h
    ${CMAKE_CURRENT_SOURCE_DIR}/alignment_selector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/alignment_selector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/allocation_properties.h
    ${CMAKE_CURRENT_SOURCE_DIR}/compression_selector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/compression_selector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_allocation_deletion.h
    ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_allocation_deletion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion.h
    ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions/engine_limits.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions/storage_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions/storage_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gfx_partition.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gfx_partition.h
    ${CMAKE_CURRENT_SOURCE_DIR}/graphics_allocation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/graphics_allocation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_defines.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/internal_allocation_storage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/internal_allocation_storage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/local_memory_usage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/local_memory_usage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_allocation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_allocation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_banks.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_operations_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_operations_status.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_pool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/migration_sync_data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/migration_sync_data.h
    ${CMAKE_CURRENT_SOURCE_DIR}/multi_graphics_allocation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/multi_graphics_allocation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_agnostic_memory_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_agnostic_memory_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/physical_address_allocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/residency.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/residency.h
    ${CMAKE_CURRENT_SOURCE_DIR}/residency_container.h
    ${CMAKE_CURRENT_SOURCE_DIR}/surface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/surface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_pooling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_pooling.h
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_reuse.h
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_reuse_cleaner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_reuse_cleaner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/page_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/page_table.h
    ${CMAKE_CURRENT_SOURCE_DIR}/page_table.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/prefetch_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/prefetch_manager.h
)

if(ENABLE_DYNAMIC_MEMORY_TRACKING)
  list(APPEND NEO_CORE_MEMORY_MANAGER ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_memory_tracking.cpp)
endif()

set_property(GLOBAL PROPERTY NEO_CORE_MEMORY_MANAGER ${NEO_CORE_MEMORY_MANAGER})
