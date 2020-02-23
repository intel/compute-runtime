#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_built_in
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/built_ins_file_names.h
  ${CMAKE_CURRENT_SOURCE_DIR}/built_ins_file_names.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/built_in_kernels_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/built_in_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/get_built_ins_file_names.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sip_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_built_in})

add_subdirectories()
