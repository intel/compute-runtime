#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_library(gmock-gtest STATIC EXCLUDE_FROM_ALL
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/gmock-gtest-all.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/gtest/gtest.h
)
target_include_directories(gmock-gtest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

