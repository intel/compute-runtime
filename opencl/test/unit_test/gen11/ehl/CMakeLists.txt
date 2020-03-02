#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_EHL)
  set(IGDRCL_SRCS_tests_gen11_ehl
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_ehl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_ehl.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen11_ehl})
  add_subdirectories()
  set(unit_test_config "ehl/1/4/8")
  include(${OPENCL_UNIT_TEST_DIR}/cmake/run_ult_target.cmake)
endif()
