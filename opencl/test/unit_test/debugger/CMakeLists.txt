#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_debugger
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/ocl_with_l0_debugger_tests.cpp
)

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_debugger})

