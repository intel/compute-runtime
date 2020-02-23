#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_PAGE_FAULT_MANAGER_WINDOWS_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_windows_tests.cpp
)

if(WIN32)
  set_property(GLOBAL PROPERTY NEO_CORE_PAGE_FAULT_MANAGER_WINDOWS_TESTS ${NEO_CORE_PAGE_FAULT_MANAGER_WINDOWS_TESTS})
endif()