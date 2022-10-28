#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_DG1)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/excludes_gen12lp_dg1.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/execution_environment_tests_dg1.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_dg1.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_tests_dg1.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_dg1.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_dg1.cpp
  )
  add_subdirectories()
endif()
