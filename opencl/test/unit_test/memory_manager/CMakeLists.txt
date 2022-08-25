#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_memory_manager
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_memory_sync_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_allocate_in_preferred_pool_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/migraton_controller_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_manager_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mem_obj_surface_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_token_tests.cpp
)

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_memory_manager})
add_subdirectories()
