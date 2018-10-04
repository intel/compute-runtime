#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN10)
  set(IGDRCL_SRCS_tests_gen10
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/coherency_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_media_kernel_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/preamble_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_source_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_queue_hw_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_cnl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit_test_helper_gen10.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen10})
  add_subdirectories()
endif()
