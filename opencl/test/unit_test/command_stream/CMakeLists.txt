#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_command_stream
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_command_stream_receiver_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_tbx_command_stream_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_1_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_2_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_mt_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_flush_task_1_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_flush_task_2_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_flush_task_3_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_flush_task_4_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_flush_task_gmock_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_with_aub_dump_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/create_command_stream_receiver_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/get_devices_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/experimental_command_buffer_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling_ocl_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/submissions_aggregator_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread_arbitration_policy_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread_arbitration_policy_helper.h
)

if(TESTS_XEHP_AND_LATER)
  list(APPEND IGDRCL_SRCS_tests_command_stream
       ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_flush_task_tests_xehp_and_later.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_xehp_and_later.cpp
  )
endif()

if(TESTS_DG2_AND_LATER)
  list(APPEND IGDRCL_SRCS_tests_command_stream
       ${CMAKE_CURRENT_SOURCE_DIR}/cache_flush_tests_dg2_and_later.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_dg2_and_later.cpp
  )
endif()

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_command_stream})
add_subdirectories()
