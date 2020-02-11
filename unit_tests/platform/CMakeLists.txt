#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_platform
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/platform_icd_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/platform_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/platform_tests_mt.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_platform})
