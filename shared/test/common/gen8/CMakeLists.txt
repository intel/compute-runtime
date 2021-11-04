#
# Copyright (C) 2019-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN8)

  set(NEO_CORE_TESTS_GEN8
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_gen8.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_gen8.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_gen8.h
  )
  set_property(GLOBAL PROPERTY NEO_CORE_TESTS_GEN8 ${NEO_CORE_TESTS_GEN8})
  add_subdirectories()

  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address_tests_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_math_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_gen8.cpp
  )

endif()
