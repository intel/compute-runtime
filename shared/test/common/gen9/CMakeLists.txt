#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN9)

  set(NEO_CORE_TESTS_GEN9
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/preamble_tests_gen9.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_gen9.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_gen9.h
  )
  set_property(GLOBAL PROPERTY NEO_CORE_TESTS_GEN9 ${NEO_CORE_TESTS_GEN9})

  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_command_encoder_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_math_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_gen9.cpp
  )

endif()
