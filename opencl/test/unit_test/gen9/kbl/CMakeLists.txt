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
  neo_copy_test_files_with_revision(copy_test_files_kbl_9 kbl 9)
  add_dependencies(copy_test_files_per_product copy_test_files_kbl_9)
endif()
