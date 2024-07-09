#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_BMG)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/engine_node_helper_tests_bmg.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe2_hpg_core_bmg.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_aot_config_tests_bmg.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_tests_bmg.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests_bmg.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_tests_bmg.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_bmg.cpp
  )

  add_subdirectories()
endif()
