#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_MEMORY_MANAGER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/address_mapper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/address_mapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/allocations_list.h
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_allocation_deletion.h
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_allocation_deletion.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion.h
  ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter.h
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics_allocation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics_allocation.h
  ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_defines.h
  ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/internal_allocation_storage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/internal_allocation_storage.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_banks.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_constants.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_pool.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_agnostic_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_agnostic_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/os_agnostic_memory_manager_allocate_in_device_pool.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table.h
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/physical_address_allocator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/residency.h
  ${CMAKE_CURRENT_SOURCE_DIR}/residency.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/residency_container.h
  ${CMAKE_CURRENT_SOURCE_DIR}/surface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/svm_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/svm_memory_manager.h
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_MEMORY_MANAGER})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_MEMORY_MANAGER ${RUNTIME_SRCS_MEMORY_MANAGER})
add_subdirectories()
