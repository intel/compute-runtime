#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_compiler_interface
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_compiler_interface_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/default_cl_cache_config_tests.cpp
)

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_compiler_interface})
