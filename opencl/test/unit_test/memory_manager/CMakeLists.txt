#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_memory_manager
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_memory_sync_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/host_ptr_manager_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/internal_allocation_storage_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_multi_device_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_allocate_in_preferred_pool_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/migraton_controller_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/surface_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_manager_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_token_tests.cpp
)

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
