#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_BMG)
  set(IGDRCL_SRCS_tests_xe2_hpg_core_bmg
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/device_info_tests_bmg.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_device_caps_bmg.cpp
  )

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe2_hpg_core_bmg})
  add_subdirectories()
endif()
