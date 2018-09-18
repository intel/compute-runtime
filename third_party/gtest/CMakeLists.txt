#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_library(gmock-gtest STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/gmock-gtest-all.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/gtest/gtest.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gmock/gmock.h
)
target_include_directories(gmock-gtest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

