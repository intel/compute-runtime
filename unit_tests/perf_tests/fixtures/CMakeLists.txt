#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_perf_tests_fixtures
    "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/command_queue_fixture.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/command_queue_fixture.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/device_fixture.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/device_fixture.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/platform_fixture.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/platform_fixture.h"
    PARENT_SCOPE
)
