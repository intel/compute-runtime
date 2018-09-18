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
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_source_level_debugger})

if(SOURCE_LEVEL_DEBUGGER_HEADERS_DIR)
  target_sources(igdrcl_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_tests.cpp)
endif()