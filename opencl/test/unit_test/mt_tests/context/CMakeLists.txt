#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_context
    # local files
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/context_usm_pool_mt_tests.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_context})
