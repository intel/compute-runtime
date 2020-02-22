#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_source_level_debugger
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_device_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_csr_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger_tests.cpp
)

get_property(NEO_CORE_TESTS_SOURCE_LEVEL_DEBUGGER GLOBAL PROPERTY NEO_CORE_TESTS_SOURCE_LEVEL_DEBUGGER)
list(APPEND IGDRCL_SRCS_tests_source_level_debugger ${NEO_CORE_TESTS_SOURCE_LEVEL_DEBUGGER})

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_source_level_debugger})