#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HP_SDV)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests_xe_hp_sdv.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_xe_hp_sdv.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_xe_hp_sdv.cpp
  )

  add_subdirectories()
endif()
