#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_BDW)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_bdw.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_bdw.cpp
  )

  add_subdirectories()
endif()
