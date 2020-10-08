#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLS)
  set(IGDRCL_SRCS_tests_gen12lp_adls
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_adls.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_adls.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_adls.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen12lp_adls})
  add_subdirectories()
  neo_copy_test_files_with_revision(copy_test_files_adls_0 adls 0)
  add_dependencies(copy_test_files_per_product copy_test_files_adls_0)
endif()
