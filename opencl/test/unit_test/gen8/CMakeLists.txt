#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN8)
  set(IGDRCL_SRCS_tests_gen8
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_gen8.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cl_hw_helper_tests_gen8.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_media_kernel_gen8.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/image_tests_gen8.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests_gen8.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_device_caps_gen8.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps_gen8.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_gen8.cpp
  )

  get_property(NEO_CORE_TESTS_GEN8 GLOBAL PROPERTY NEO_CORE_TESTS_GEN8)
  list(APPEND IGDRCL_SRCS_tests_gen8 ${NEO_CORE_TESTS_GEN8})

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen8})
  add_subdirectories()
endif()
