#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_SKL)
  set(IGDRCL_SRCS_offline_compiler_tests_skl
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/offline_compiler_tests_skl.cpp
  )
  target_sources(ocloc_tests PRIVATE ${IGDRCL_SRCS_offline_compiler_tests_skl})

endif()
