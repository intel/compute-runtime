#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN12LP)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_tests_gen12lp.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_simulated_common_hw_tests_gen12lp.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests_gen12lp.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/excludes_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gen12lp_shared_tests_wrapper.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_cmds_gen12lp_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_command_encoder_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_math_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_gen12lp.cpp
  )

  add_subdirectories()
endif()
