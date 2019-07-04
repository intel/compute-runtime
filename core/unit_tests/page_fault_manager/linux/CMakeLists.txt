#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_PAGE_FAULT_MANAGER_LINUX_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_linux_tests.cpp
)

if(UNIX)
  set_property(GLOBAL PROPERTY NEO_CORE_PAGE_FAULT_MANAGER_LINUX_TESTS ${NEO_CORE_PAGE_FAULT_MANAGER_LINUX_TESTS})
endif()