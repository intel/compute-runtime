#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_SHARED_SRCS_mt_tests_command_stream
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_mt_tests.cpp
)
target_sources(neo_shared_mt_tests PRIVATE ${NEO_SHARED_SRCS_mt_tests_command_stream})
