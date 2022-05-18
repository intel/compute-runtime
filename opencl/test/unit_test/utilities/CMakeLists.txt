#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/file_logger_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_logger_tests.h
)

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_utilities})
