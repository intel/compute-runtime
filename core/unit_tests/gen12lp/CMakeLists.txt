#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_TESTS_GEN12LP
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_gen12lp.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_gen12lp.cpp
)
set_property(GLOBAL PROPERTY NEO_CORE_TESTS_GEN12LP ${NEO_CORE_TESTS_GEN12LP})
add_subdirectories()