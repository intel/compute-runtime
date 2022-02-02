#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_DG2)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests_dg2.cpp
  )
endif()

