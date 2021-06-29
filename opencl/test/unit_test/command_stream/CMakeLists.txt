#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_command_stream
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_1_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_2_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_3_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_file_stream_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_subcapture_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_1_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_2_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_mt_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_flush_task_1_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_flush_task_2_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_flush_task_3_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_flush_task_4_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_flush_task_gmock_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_with_aub_dump_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_simulated_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/create_command_stream_receiver_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/get_devices_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/experimental_command_buffer_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/submissions_aggregator_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_fixture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_stream_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/thread_arbitration_policy_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread_arbitration_policy_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests.h
)

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_command_stream})
add_subdirectories()
