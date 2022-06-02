#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLN)
  set(IGDRCL_SRCS_tests_gen12lp_adln_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_adln.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_gen12lp_adln_excludes})

  set(IGDRCL_SRCS_tests_gen12lp_adln
      ${IGDRCL_SRCS_tests_gen12lp_adln_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen12lp_adln})
endif()