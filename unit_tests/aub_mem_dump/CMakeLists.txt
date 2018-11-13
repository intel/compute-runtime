#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_aub_mem_dump_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/lrca_helper_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_aub_mem_dump_tests})
