#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_DG2)
  set(IGDRCL_SRCS_tests_xe_hpg_core_dg2_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_dg2.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_xe_hpg_core_dg2_excludes})

  set(IGDRCL_SRCS_tests_xe_hpg_core_dg2
      ${IGDRCL_SRCS_tests_xe_hpg_core_dg2_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/command_queue_tests_dg2.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cmds_programming_dg2.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_device_caps_dg2.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe_hpg_core_dg2})
  add_subdirectories()
endif()
