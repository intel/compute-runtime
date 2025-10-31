#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_SHARED_SRCS_mt_tests_host_function
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function_tests_mt.cpp
)
target_sources(neo_shared_mt_tests PRIVATE ${NEO_SHARED_SRCS_mt_tests_host_function})
