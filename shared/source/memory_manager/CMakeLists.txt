#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_MEMORY_MANAGER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/allocations_list.h
  ${CMAKE_CURRENT_SOURCE_DIR}/allocation_properties.h
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_allocation_deletion.h
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_allocation_deletion.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion.h
  ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter.h
  ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}/engine_limits.h
  ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}/storage_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}/storage_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/eviction_status.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gfx_partition.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_DIR_SUFFIX}/gfx_partition_init_additional_range.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gfx_partition.h
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics_allocation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics_allocation.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/graphics_allocation_extra.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_defines.h
  ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/internal_allocation_storage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/internal_allocation_storage.h
  ${CMAKE_CURRENT_SOURCE_DIR}/local_memory_usage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/local_memory_usage.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_constants.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_operations_handler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_operations_status.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_pool.h
  ${CMAKE_CURRENT_SOURCE_DIR}/residency.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/residency.h
  ${CMAKE_CURRENT_SOURCE_DIR}/residency_container.h
  ${CMAKE_CURRENT_SOURCE_DIR}/surface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_manager.h
)

set_property(GLOBAL PROPERTY NEO_CORE_MEMORY_MANAGER ${NEO_CORE_MEMORY_MANAGER})
