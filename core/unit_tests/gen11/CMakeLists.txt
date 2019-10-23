#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_TESTS_GEN11
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/preamble_tests_gen11.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_gen11.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_gen11.cpp
)
set_property(GLOBAL PROPERTY NEO_CORE_TESTS_GEN11 ${NEO_CORE_TESTS_GEN11})
add_subdirectories()