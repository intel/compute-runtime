#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.2.0 FATAL_ERROR)

add_subdirectory(api)
add_subdirectory(fixtures)

# Setting up our local list of test files
set(IGDRCL_SRCS_performance_tests
    ${IGDRCL_SRCS_perf_tests_api}
    ${IGDRCL_SRCS_perf_tests_fixtures}
    "${CMAKE_CURRENT_SOURCE_DIR}/options_perf_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/perf_test_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/perf_test_utils.h"
    PARENT_SCOPE
)


