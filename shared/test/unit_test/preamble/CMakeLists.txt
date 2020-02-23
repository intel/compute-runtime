#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_PREAMBLE_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/preamble_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preamble_tests.cpp
)
set_property(GLOBAL PROPERTY NEO_CORE_PREAMBLE_TESTS ${NEO_CORE_PREAMBLE_TESTS})
