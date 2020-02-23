#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_TESTS_SOURCE_LEVEL_DEBUGGER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_preamble_test.h
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_preamble_test.inl
)
set_property(GLOBAL PROPERTY NEO_CORE_TESTS_SOURCE_LEVEL_DEBUGGER ${NEO_CORE_TESTS_SOURCE_LEVEL_DEBUGGER})