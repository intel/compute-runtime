#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set_property(GLOBAL APPEND PROPERTY LEO_TEST_SOURCES
             ${CMAKE_CURRENT_SOURCE_DIR}/test_command_queue.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_command_queue_ooq.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_command_queue_dependencies.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_command_queue_throttle.cpp
)
