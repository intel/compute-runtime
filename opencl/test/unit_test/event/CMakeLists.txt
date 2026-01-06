#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_event
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/event_builder_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/event_callbacks_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/event_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/event_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/user_events_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_event})
