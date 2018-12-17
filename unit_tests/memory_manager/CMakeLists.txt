#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_memory_manager
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/address_mapper_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_allocation_deletion_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter_mt_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/graphics_allocation_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_manager_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/internal_allocation_storage_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/memory_manager_allocate_in_device_pool_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_allocate_in_device_pool_tests.inl
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/memory_manager_allocate_in_preferred_pool_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_allocate_in_preferred_pool_tests.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_pool_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/physical_address_allocator_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/surface_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/svm_memory_manager_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_memory_manager})
add_subdirectories()
