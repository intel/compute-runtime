#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_gen_common
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/exclude_tests/exclude_test_declare.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/exclude_tests/exclude_test_exclude.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen_commands_common_validation.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}/test.h
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen_common})

add_subdirectories()
