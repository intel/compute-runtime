#
# Copyright (C) 2019-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN12LP)

  set(NEO_CORE_TESTS_GEN12LP
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_gen12lp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_gen12lp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_gen12lp.h
  )
  set_property(GLOBAL PROPERTY NEO_CORE_TESTS_GEN12LP ${NEO_CORE_TESTS_GEN12LP})
  add_subdirectories()

  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_command_encoder_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_math_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_gen12lp.cpp
  )

endif()
