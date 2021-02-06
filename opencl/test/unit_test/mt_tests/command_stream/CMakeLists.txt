#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_command_stream
    # local files
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt

    # necessary dependencies from igdrcl_tests
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/command_stream/command_stream_receiver_mt_tests.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_command_stream})
