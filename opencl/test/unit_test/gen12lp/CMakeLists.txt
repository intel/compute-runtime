#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN12LP)
  set(IGDRCL_SRCS_tests_gen12lp_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_gen12lp.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_gen12lp_excludes})

  set(IGDRCL_SRCS_tests_gen12lp
      ${IGDRCL_SRCS_tests_gen12lp_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/buffer_tests_gen12lp.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_gen12lp.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/gen12lp_tests_wrapper.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/gpgpu_walker_tests_gen12lp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_tests_gen12lp.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/image_tests_gen12lp.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests_gen12lp.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/profiling_tests_gen12lp.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_device_caps_gen12lp.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps_gen12lp.inl
  )

  get_property(NEO_CORE_TESTS_GEN12LP GLOBAL PROPERTY NEO_CORE_TESTS_GEN12LP)
  list(APPEND IGDRCL_SRCS_tests_gen12lp ${NEO_CORE_TESTS_GEN12LP})

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen12lp})
  add_subdirectories()
endif()
