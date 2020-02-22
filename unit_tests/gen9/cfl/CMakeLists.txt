#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_CFL)
  set(IGDRCL_SRCS_tests_gen9_cfl
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_cfl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_cfl.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen9_cfl})
  add_subdirectories()
endif()
