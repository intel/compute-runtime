#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_SHARED_SRCS_mt_tests_direct_submission
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_controller_tests_mt.cpp
)
target_sources(neo_shared_mt_tests PRIVATE ${NEO_SHARED_SRCS_mt_tests_direct_submission})
