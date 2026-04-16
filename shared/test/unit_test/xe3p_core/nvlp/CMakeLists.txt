#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_NVLP)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/blit_commands_helper_tests_nvlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/engine_node_helper_tests_nvlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe3p_core_nvlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_tests_nvlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests_nvlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_aot_config_tests_nvlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_tests_nvlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_nvlp.cpp
  )

  add_subdirectories()
endif()
