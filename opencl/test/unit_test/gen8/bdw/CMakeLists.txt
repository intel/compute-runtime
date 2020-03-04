#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_BDW)
  set(IGDRCL_SRCS_tests_gen8_bdw
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tests_bdw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_bdw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_bdw.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen8_bdw})
  add_subdirectories()
  set(appverified_allowed ${APPVERIFIER_ALLOWED})
  set(unit_test_config "bdw/1/3/8")
  include(${OPENCL_UNIT_TEST_DIR}/cmake/run_ult_target.cmake)
endif()
