#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_MEMORY_MANAGER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/address_mapper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/address_mapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_memory_sync.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_banks.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_agnostic_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_agnostic_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/os_agnostic_memory_manager_allocate_in_device_pool.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table.h
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/physical_address_allocator.h
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_MEMORY_MANAGER})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_MEMORY_MANAGER ${RUNTIME_SRCS_MEMORY_MANAGER})
add_subdirectories()
