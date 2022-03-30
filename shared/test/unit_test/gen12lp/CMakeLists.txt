#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN12LP)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_tests_gen12lp.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_simulated_common_hw_tests_gen12lp.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests_gen12lp.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/gen12lp_shared_tests_wrapper.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_cmds_gen12lp_tests.cpp
  )
  add_subdirectories()
endif()
