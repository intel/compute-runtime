#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN11)
  target_sources(igdrcl_aub_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration_gen11.cpp
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/gen11/unit_test_helper_gen11.cpp
  )
  add_subdirectories()
endif()
