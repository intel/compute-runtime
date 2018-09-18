#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_gmm_helper
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_helper_tests.cpp
)
if(NOT USE_STATIC_GMM)
  list(APPEND IGDRCL_SRCS_tests_gmm_helper ${CMAKE_CURRENT_SOURCE_DIR}/gmm_interface_tests.cpp)
endif()
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gmm_helper})
add_subdirectories()
