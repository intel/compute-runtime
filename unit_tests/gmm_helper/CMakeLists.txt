#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_gmm_helper
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_helper_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_interface_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gmm_helper})
add_subdirectories()
