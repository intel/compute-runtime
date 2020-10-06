#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_RKL)
  set(IGDRCL_SRCS_tests_gen12lp_rkl
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_helper_rkl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_rkl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_rkl.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen12lp_rkl})
  add_subdirectories()
  neo_copy_test_files_with_revision(copy_test_files_rkl_0 rkl 0)
  add_dependencies(copy_test_files_per_product copy_test_files_rkl_0)
endif()
