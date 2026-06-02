#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set_property(GLOBAL APPEND PROPERTY LEO_TEST_SOURCES
             ${CMAKE_CURRENT_SOURCE_DIR}/test_api_command_queue.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_api_perf_counters.cpp
)
