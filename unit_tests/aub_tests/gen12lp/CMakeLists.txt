#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN12LP)
  target_sources(igdrcl_aub_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump_tests_gen12lp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration_gen12lp.cpp
    ${NEO_SOURCE_DIR}/unit_tests/gen12lp/special_ult_helper_gen12lp.cpp
    ${NEO_SOURCE_DIR}/unit_tests/gen12lp/unit_test_helper_gen12lp.cpp
  )
  add_subdirectories()
endif()
