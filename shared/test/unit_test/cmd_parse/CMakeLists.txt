#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_SHARED_TESTS_CMD_PARSE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_base.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_base_mi_arb.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_compute_mode.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_gpgpu_walker.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_mi_arb.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_sip.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/gen_cmd_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_parse.inl
)

set_property(GLOBAL PROPERTY NEO_SHARED_TESTS_CMD_PARSE ${NEO_SHARED_TESTS_CMD_PARSE})
