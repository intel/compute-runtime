#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN8)
  target_sources(igdrcl_aub_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration_gen8.cpp
    ${NEO_SOURCE_DIR}/unit_tests/gen8/unit_test_helper_gen8.cpp
  )
  add_subdirectories()
endif()
