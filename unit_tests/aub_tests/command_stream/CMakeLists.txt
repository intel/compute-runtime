#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(igdrcl_aub_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump_tests.cpp
)
