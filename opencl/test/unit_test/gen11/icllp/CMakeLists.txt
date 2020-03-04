#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ICLLP)
  set(IGDRCL_SRCS_tests_gen11_icllp
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_icllp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_icllp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_media_kernel_icllp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_program_media_sampler_icllp.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen11_icllp})
  add_subdirectories()
  set(appverified_allowed ${APPVERIFIER_ALLOWED})
  set(unit_test_config "icllp/1/8/8")
  include(${OPENCL_UNIT_TEST_DIR}/cmake/run_ult_target.cmake)
endif()
