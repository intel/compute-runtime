#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_PAGE_FAULT_MANAGER_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_linux.h
)

if(UNIX)
  set_property(GLOBAL PROPERTY NEO_CORE_PAGE_FAULT_MANAGER_LINUX ${NEO_CORE_PAGE_FAULT_MANAGER_LINUX})
endif()