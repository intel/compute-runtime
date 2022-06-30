#
# Copyright (C) 2021-2022 Intel Corporation
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
      ${CMAKE_CURRENT_SOURCE_DIR}/get_device_info_dg2.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_tests_dg2.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests_dg2.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cmds_programming_dg2.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_dg2.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe_hpg_core_dg2})
  add_subdirectories()
  neo_copy_test_files_with_revision(copy_test_files_dg2_0 dg2 0)
  add_dependencies(copy_test_files_per_product copy_test_files_dg2_0)
endif()
