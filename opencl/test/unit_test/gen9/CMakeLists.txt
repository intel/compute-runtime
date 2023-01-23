#
# Copyright (C) 2018-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN9)
  set(IGDRCL_SRCS_tests_gen9
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cl_get_platform_ids_tests_gen9.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cl_gfx_core_helper_tests_gen9.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_gen9.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel_gen9.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_media_kernel_gen9.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/image_tests_gen9.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests_gen9.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_device_caps_gen9.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps_gen9.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_gen9.cpp
  )

  get_property(NEO_CORE_TESTS_GEN9 GLOBAL PROPERTY NEO_CORE_TESTS_GEN9)
  list(APPEND IGDRCL_SRCS_tests_gen9 ${NEO_CORE_TESTS_GEN9})

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen9})
  add_subdirectories()
endif()
