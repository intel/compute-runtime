#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_command_stream
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_mt_tests.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_command_stream})
