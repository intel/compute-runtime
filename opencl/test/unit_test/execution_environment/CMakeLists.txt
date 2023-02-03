#
# Copyright (C) 2018-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_execution_environment
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_execution_environment_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_execution_environment})
