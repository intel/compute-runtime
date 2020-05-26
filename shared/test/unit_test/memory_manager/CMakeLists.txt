#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_MEMORY_MANAGER_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/multi_graphics_allocation_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_MEMORY_MANAGER_TESTS ${NEO_CORE_MEMORY_MANAGER_TESTS})
