#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_RKL)
  set(IGDRCL_SRCS_tests_gen12lp_rkl
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_rkl.cpp
  )
  target_sources(neo_shared_tests PRIVATE ${IGDRCL_SRCS_tests_gen12lp_rkl})
  add_subdirectories()
endif()
