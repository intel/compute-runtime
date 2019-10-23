#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_TESTS_GEN9
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/preamble_tests_gen9.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_gen9.cpp
)
set_property(GLOBAL PROPERTY NEO_CORE_TESTS_GEN9 ${NEO_CORE_TESTS_GEN9})
add_subdirectories()