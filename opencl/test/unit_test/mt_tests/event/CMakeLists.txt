#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_event
    # local files
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/event_tests_mt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/user_events_tests_mt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/async_events_handler_mt_tests.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_event})
