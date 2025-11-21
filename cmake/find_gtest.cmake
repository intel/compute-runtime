#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

get_filename_component(GTEST_DIR "${NEO_SOURCE_DIR}/third_party/gtest" ABSOLUTE)
if(IS_DIRECTORY ${GTEST_DIR})

  add_library(neo_gtest STATIC EXCLUDE_FROM_ALL
              ${NEO_SOURCE_DIR}/cmake/find_gtest.cmake
              ${GTEST_DIR}/src/gtest-all.cc
  )
  target_include_directories(neo_gtest PRIVATE ${GTEST_DIR})
  target_include_directories(neo_gtest PUBLIC ${GTEST_DIR}/include)
endif()
