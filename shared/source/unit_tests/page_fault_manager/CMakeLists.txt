#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_CPU_PAGE_FAULT_MANAGER_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_tests_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/default_asan_options.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_cpu_page_fault_manager.h
)

set_property(GLOBAL PROPERTY NEO_CORE_CPU_PAGE_FAULT_MANAGER_TESTS ${NEO_CORE_CPU_PAGE_FAULT_MANAGER_TESTS})

add_subdirectories()