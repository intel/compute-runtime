#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN12LP)
  set(IGDRCL_SRCS_tests_gen12lp
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_tests_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/coherency_tests_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_simulated_common_hw_tests_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_media_kernel_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/gen12lp_tests_wrapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware_commands_helper_tests_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/image_tests_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/profiling_tests_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_source_tests_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/special_ult_helper_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/special_ult_helper_gen12lp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver_tests_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_queue_hw_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/unit_test_helper_gen12lp.cpp
  )

  get_property(NEO_CORE_TESTS_GEN12LP GLOBAL PROPERTY NEO_CORE_TESTS_GEN12LP)
  list(APPEND IGDRCL_SRCS_tests_gen12lp ${NEO_CORE_TESTS_GEN12LP})

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen12lp})
  add_subdirectories()
endif()
