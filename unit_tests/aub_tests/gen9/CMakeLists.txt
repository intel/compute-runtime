#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN9)
  target_sources(igdrcl_aub_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration_gen9.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/gen9/unit_test_helper_gen9.cpp
  )
  add_subdirectories()
endif()
