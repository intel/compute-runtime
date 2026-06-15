#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_SHARED_SRCS_mt_tests_device
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/peer_access_mt_tests.cpp
)

target_sources(neo_shared_mt_tests PRIVATE ${NEO_SHARED_SRCS_mt_tests_device})
