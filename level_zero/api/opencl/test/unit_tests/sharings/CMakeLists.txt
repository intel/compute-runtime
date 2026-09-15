#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set_property(GLOBAL APPEND PROPERTY LEO_TEST_SOURCES
             ${CMAKE_CURRENT_SOURCE_DIR}/test_leo_sharing_factory.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_leo_sharing_handler.cpp
)

add_subdirectories()
