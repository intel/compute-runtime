#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set_property(GLOBAL APPEND PROPERTY LEO_TEST_SOURCES
             ${CMAKE_CURRENT_SOURCE_DIR}/test_event.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_event_ooq.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_regular_events_manager.cpp
)
