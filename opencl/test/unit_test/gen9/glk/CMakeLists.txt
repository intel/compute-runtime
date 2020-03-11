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
  neo_copy_test_files(copy_test_files_glk glk)
  add_dependencies(unit_tests copy_test_files_glk)
endif()
