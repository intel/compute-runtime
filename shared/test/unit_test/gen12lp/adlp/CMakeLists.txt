#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLP)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/preamble_helper_tests_adlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_adlp.cpp
  )

  add_subdirectories()
endif()
