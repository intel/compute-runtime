#
# Copyright (C) 2019-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN11)
  set(IGDRCL_SRCS_tests_gen11
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cl_gfx_core_helper_tests_gen11.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_gen11.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel_gen11.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_media_kernel_gen11.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/image_tests_gen11.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests_gen11.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_device_caps_gen11.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps_gen11.cpp
  )

  get_property(NEO_CORE_TESTS_GEN11 GLOBAL PROPERTY NEO_CORE_TESTS_GEN11)
  list(APPEND IGDRCL_SRCS_tests_gen11 ${NEO_CORE_TESTS_GEN11})

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen11})
  add_subdirectories()
endif()
