#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_TESTS_GEN8
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_gen8.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_gen8.cpp
)
set_property(GLOBAL PROPERTY NEO_CORE_TESTS_GEN8 ${NEO_CORE_TESTS_GEN8})
add_subdirectories()