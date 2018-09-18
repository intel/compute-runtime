#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_scenarios
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/blocked_enqueue_barrier_scenario_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/blocked_enqueue_with_callback_scenario_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_scenarios})