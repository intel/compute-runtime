#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set_property(GLOBAL APPEND PROPERTY LEO_TEST_SOURCES
             ${CMAKE_CURRENT_SOURCE_DIR}/capturing_command_list.h
             ${CMAKE_CURRENT_SOURCE_DIR}/capturing_command_list_args.h
             ${CMAKE_CURRENT_SOURCE_DIR}/leo_capture_fixture.h
             ${CMAKE_CURRENT_SOURCE_DIR}/leo_capture_fixture.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/ocl_fixture.h
             ${CMAKE_CURRENT_SOURCE_DIR}/ocl_fixture.cpp
)
