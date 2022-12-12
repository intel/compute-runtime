#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager_tests_dg2.cpp
  )
  add_subdirectories()
endif()