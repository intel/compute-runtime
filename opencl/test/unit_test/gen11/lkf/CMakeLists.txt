#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_LKF)
  set(IGDRCL_SRCS_tests_gen11_lkf
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_lkf.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen11_lkf})
  add_subdirectories()
  neo_copy_test_files_with_revision(copy_test_files_lkf_0 lkf 0)
  add_dependencies(copy_test_files_per_product copy_test_files_lkf_0)
endif()
