#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_source_level_debugger
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_device_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_csr_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_preamble_test.h
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_preamble_test.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_source_level_debugger})