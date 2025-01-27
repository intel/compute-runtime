#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PTL)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe3_core_ptl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_aot_config_tests_ptl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_tests_ptl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests_ptl.cpp
  )

  add_subdirectories()
endif()
