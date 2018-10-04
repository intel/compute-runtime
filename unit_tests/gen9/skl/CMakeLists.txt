#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_SKL)
  set(IGDRCL_SRCS_tests_gen9_skl
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tests_skl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_skl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_skl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_skl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_skl.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen9_skl})
  add_subdirectories()
endif()
