#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_CPU_PAGE_FAULT_MANAGER_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/default_asan_options.cpp
)
set_property(GLOBAL PROPERTY NEO_CORE_CPU_PAGE_FAULT_MANAGER_TESTS ${NEO_CORE_CPU_PAGE_FAULT_MANAGER_TESTS})

add_subdirectories()