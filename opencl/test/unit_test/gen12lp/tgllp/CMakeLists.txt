#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_TGLLP)
  set(IGDRCL_SRCS_tests_gen12lp_tgllp
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests_tgllp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_helper_tgllp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_tgllp.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen12lp_tgllp})
  add_subdirectories()
  neo_copy_test_files(copy_test_files_tgllp tgllp)
  add_dependencies(unit_tests copy_test_files_tgllp)
endif()
