#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_BDW)
  set(IGDRCL_SRCS_tests_gen8_bdw
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_bdw.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_bdw.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen8_bdw})
  add_subdirectories()
  neo_copy_test_files_with_revision(copy_test_files_bdw_0 bdw 0)
  add_dependencies(copy_test_files_per_product copy_test_files_bdw_0)
endif()
