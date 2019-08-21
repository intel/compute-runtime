#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_AUB
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_memory_operations_handler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_memory_operations_handler.h
)

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_AUB ${NEO_CORE_OS_INTERFACE_AUB})
add_subdirectories()
