#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_PAGE_FAULT_MANAGER_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_windows.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_windows.h
)

if(WIN32)
  set_property(GLOBAL PROPERTY NEO_CORE_PAGE_FAULT_MANAGER_WINDOWS ${NEO_CORE_PAGE_FAULT_MANAGER_WINDOWS})
endif()