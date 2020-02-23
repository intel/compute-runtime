#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_PAGE_FAULT_MANAGER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager.h
)

set_property(GLOBAL PROPERTY NEO_CORE_PAGE_FAULT_MANAGER ${NEO_CORE_PAGE_FAULT_MANAGER})

add_subdirectories()