#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_scheduler
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_kernel_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_source_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_source_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_source_tests.inl
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_scheduler})
