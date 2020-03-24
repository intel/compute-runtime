#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_test_macros
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/test_checks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_checks.h
)

set_property(GLOBAL PROPERTY NEO_CORE_test_macros ${NEO_CORE_test_macros})
