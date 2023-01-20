#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_RKL)
  set(IGDRCL_SRCS_tests_gen12lp_rkl_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_rkl.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_gen12lp_rkl_excludes})

  add_subdirectories()
endif()
