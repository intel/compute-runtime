#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_sharings_unified
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_sharing_buffer_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_sharing_fixtures.h
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_sharing_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_sharings_unified})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_sharings_unified ${IGDRCL_SRCS_tests_sharings_unified})
add_subdirectories()
