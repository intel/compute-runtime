#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_KBL)
  set(IGDRCL_SRCS_tests_gen9_kbl
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_kbl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_kbl.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen9_kbl})
  add_subdirectories()
endif()
