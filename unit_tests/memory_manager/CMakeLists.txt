#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_memory_manager
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/address_mapper_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_memory_sync_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_allocation_deletion_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter_mt_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_DIR_SUFFIX}/gfx_partition_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gfx_partition_tests.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics_allocation_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_manager_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/internal_allocation_storage_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/local_memory_usage_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_multi_device_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/memory_manager_allocate_in_device_pool_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_allocate_in_device_pool_tests.inl
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/memory_manager_allocate_in_preferred_pool_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_allocate_in_preferred_pool_tests.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_pool_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/physical_address_allocator_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/surface_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_manager_tests.cpp
)

get_property(NEO_CORE_UNIFIED_MEMORY_TESTS GLOBAL PROPERTY NEO_CORE_UNIFIED_MEMORY_TESTS)
list(APPEND IGDRCL_SRCS_tests_memory_manager ${NEO_CORE_UNIFIED_MEMORY_TESTS})

get_property(NEO_CORE_CPU_PAGE_FAULT_MANAGER_TESTS GLOBAL PROPERTY NEO_CORE_CPU_PAGE_FAULT_MANAGER_TESTS)
list(APPEND IGDRCL_SRCS_tests_memory_manager ${NEO_CORE_CPU_PAGE_FAULT_MANAGER_TESTS})

if(WIN32)
  get_property(NEO_CORE_PAGE_FAULT_MANAGER_WINDOWS_TESTS GLOBAL PROPERTY NEO_CORE_PAGE_FAULT_MANAGER_WINDOWS_TESTS)
  list(APPEND IGDRCL_SRCS_tests_memory_manager ${NEO_CORE_PAGE_FAULT_MANAGER_WINDOWS_TESTS})
endif()

if(UNIX)
  get_property(NEO_CORE_PAGE_FAULT_MANAGER_LINUX_TESTS GLOBAL PROPERTY NEO_CORE_PAGE_FAULT_MANAGER_LINUX_TESTS)
  list(APPEND IGDRCL_SRCS_tests_memory_manager ${NEO_CORE_PAGE_FAULT_MANAGER_LINUX_TESTS})
endif()

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_memory_manager})
add_subdirectories()
