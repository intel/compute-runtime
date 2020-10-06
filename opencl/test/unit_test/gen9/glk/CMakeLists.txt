#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GLK)
  set(IGDRCL_SRCS_tests_gen9_glk
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_glk.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_glk.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen9_glk})
  add_subdirectories()
  neo_copy_test_files_with_revision(copy_test_files_glk_0 glk 0)
  add_dependencies(copy_test_files_per_product copy_test_files_glk_0)
endif()
