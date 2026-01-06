#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_CRI)
  set(IGDRCL_SRCS_tests_xe3p_core_cri
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/device_info_tests_cri.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_cri.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/program_tests_cri.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_device_caps_cri.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cmds_programming_cri.cpp
  )

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe3p_core_cri})
endif()
