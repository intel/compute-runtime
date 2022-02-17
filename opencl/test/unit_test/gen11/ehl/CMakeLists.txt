#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_EHL)
  set(IGDRCL_SRCS_tests_gen11_ehl_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_ehl.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_gen11_ehl_excludes})
  set(IGDRCL_SRCS_tests_gen11_ehl
      ${IGDRCL_SRCS_tests_gen11_ehl_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_ehl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_ehl.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen11_ehl})
  add_subdirectories()
  neo_copy_test_files_with_revision(copy_test_files_ehl_0 ehl 0)
  add_dependencies(copy_test_files_per_product copy_test_files_ehl_0)
endif()
