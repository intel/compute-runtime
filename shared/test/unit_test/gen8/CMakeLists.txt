#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN8)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/coherency_tests_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/state_base_address_tests_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_math_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_gen8.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_gen8.cpp
  )

  add_subdirectories()
endif()
