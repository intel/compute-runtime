#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_execution_model
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_execution_model_kernel_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/parent_kernel_dispatch_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_dispatch_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/submit_blocked_parent_kernel_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_execution_model})
