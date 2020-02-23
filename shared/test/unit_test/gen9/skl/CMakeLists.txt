#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_TESTS_GEN9_SKL
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_skl.cpp
)
set_property(GLOBAL PROPERTY NEO_CORE_TESTS_GEN9_SKL ${NEO_CORE_TESTS_GEN9_SKL})
add_subdirectories()
