#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN11)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/coherency_tests_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_cmds_gen11_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/preamble_tests_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_math_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_gen11.cpp
  )

  add_subdirectories()
endif()
