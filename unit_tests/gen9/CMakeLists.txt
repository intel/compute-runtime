#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN9)
  set(IGDRCL_SRCS_tests_gen9
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/coherency_tests_gen9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_gen9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel_gen9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_media_kernel_gen9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_gen9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_tests_gen9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests_gen9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests_gen9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_source_tests_gen9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sip_tests_gen9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_gen9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_queue_hw_gen9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps_gen9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_gen9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit_test_helper_gen9.cpp
  )

  get_property(NEO_CORE_TESTS_GEN9 GLOBAL PROPERTY NEO_CORE_TESTS_GEN9)
  list(APPEND IGDRCL_SRCS_tests_gen9 ${NEO_CORE_TESTS_GEN9})

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen9})
  add_subdirectories()
endif()
