#
# Copyright (C) 2019-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN11)
  set(NEO_CORE_TESTS_GEN11
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/preamble_tests_gen11.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_gen11.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_gen11.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_gen11.h
  )
  set_property(GLOBAL PROPERTY NEO_CORE_TESTS_GEN11 ${NEO_CORE_TESTS_GEN11})
  add_subdirectories()

  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_math_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_gen11.cpp
  )

endif()
