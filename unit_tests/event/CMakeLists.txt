#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_event
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/async_events_handler_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/event_builder_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/event_callbacks_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/event_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/event_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/event_tests_mt.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/event_tracker_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/user_events_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/user_events_tests_mt.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_event})
