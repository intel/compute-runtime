#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_SRCS_tests_program
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/program_info_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program_info_from_patchtokens_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/program_initialization_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_SRCS_tests_program ${NEO_CORE_SRCS_tests_program})
add_subdirectories()
