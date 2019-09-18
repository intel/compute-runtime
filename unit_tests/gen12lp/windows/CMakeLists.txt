#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_gen12lp_windows
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests_gen12lp.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_callbacks_tests_gen12lp.cpp
)
if(WIN32)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen12lp_windows})
  add_subdirectories()
endif()
