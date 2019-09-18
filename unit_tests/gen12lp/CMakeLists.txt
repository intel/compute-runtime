#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN12LP)
  set(IGDRCL_SRCS_tests_gen12lp
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_tests_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/coherency_tests_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_simulated_common_hw_tests_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_media_kernel_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_tests_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profiling_tests_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_source_tests_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver_tests_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_queue_hw_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit_test_helper_gen12lp.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen12lp})
  add_subdirectories()
endif()
