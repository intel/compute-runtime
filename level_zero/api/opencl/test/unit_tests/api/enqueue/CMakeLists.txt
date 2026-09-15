#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set_property(GLOBAL APPEND PROPERTY LEO_TEST_SOURCES
             ${CMAKE_CURRENT_SOURCE_DIR}/test_api_enqueue.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_api_enqueue_fill_image.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_api_enqueue_events.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_api_enqueue_rect.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_api_enqueue_validation.cpp
)
