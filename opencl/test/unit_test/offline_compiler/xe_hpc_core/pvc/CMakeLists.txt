#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PVC)
  set(IGDRCL_SRCS_offline_compiler_tests_pvc
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/offline_compiler_tests_pvc.cpp
  )
  target_sources(ocloc_tests PRIVATE ${IGDRCL_SRCS_offline_compiler_tests_pvc})

endif()
