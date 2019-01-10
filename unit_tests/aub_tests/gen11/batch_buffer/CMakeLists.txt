#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(igdrcl_aub_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_batch_buffer_tests_gen11.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_batch_buffer_tests_gen11.cpp
)
