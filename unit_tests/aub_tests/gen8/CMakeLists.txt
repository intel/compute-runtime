#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN8)
  target_sources(igdrcl_aub_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration.cpp
  )
  add_subdirectories()
endif()
