#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_CRI)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_file_stream_tests_cri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/engine_node_helper_tests_cri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe3p_core_cri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_tests_cri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests_cri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_tests_cri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_cri.cpp
  )

  add_subdirectories()
endif()
