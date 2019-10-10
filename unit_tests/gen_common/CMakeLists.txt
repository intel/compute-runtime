#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_gen_common
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_base.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_base_mi_arb.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_compute_mode.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_gpgpu_walker.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_mi_arb.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_sip.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/exclude_tests/exclude_test_declare.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/exclude_tests/exclude_test_exclude.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gen_cmd_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gen_commands_common_validation.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_cmds_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/matchers.h
  ${NEO_CORE_DIRECTORY}/unit_tests/test_macros${BRANCH_DIR_SUFFIX}/test.h
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen_common})

add_subdirectories()
