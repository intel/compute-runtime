#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(igdrcl_aub_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_parent_kernel_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/fixture_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hello_world_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/run_kernel_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/simple_arg_fixture.h
)
