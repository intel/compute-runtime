#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}debug_file_reader_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_file_reader_tests.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_reader_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_logger_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_logger_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tag_allocator_tests.cpp
)

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_utilities})
