#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN8)
  set(IGDRCL_SRCS_tests_gen8
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/coherency_tests_gen8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_gen8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_media_kernel_gen8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_gen8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_tests_gen8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests_gen8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests_gen8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_dispatch_tests_gen8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_source_tests_gen8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_gen8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_queue_hw_gen8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps_gen8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_gen8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit_test_helper_gen8.cpp
  )

  get_property(NEO_CORE_TESTS_GEN8 GLOBAL PROPERTY NEO_CORE_TESTS_GEN8)
  list(APPEND IGDRCL_SRCS_tests_gen8 ${NEO_CORE_TESTS_GEN8})

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen8})
  add_subdirectories()
endif()
