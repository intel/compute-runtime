#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN9)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_cmds_gen9_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_tests_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/preamble_tests_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sip_tests_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_command_encoder_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_math_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_gen9.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_gen9.cpp
  )

  add_subdirectories()
endif()
