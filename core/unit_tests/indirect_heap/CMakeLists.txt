#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_INDIRECT_HEAP_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/indirect_heap_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_INDIRECT_HEAP_TESTS ${NEO_CORE_INDIRECT_HEAP_TESTS})
add_subdirectories()
