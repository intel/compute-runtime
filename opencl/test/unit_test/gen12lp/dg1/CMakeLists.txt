#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_DG1)
  set(IGDRCL_SRCS_tests_dg1
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_dg1.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_tests_dg1.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_dg1.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_dg1.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_dg1.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_dg1})
  add_subdirectories()
  neo_copy_test_files(copy_test_files_dg1 dg1)
  add_dependencies(unit_tests copy_test_files_dg1)
endif()
