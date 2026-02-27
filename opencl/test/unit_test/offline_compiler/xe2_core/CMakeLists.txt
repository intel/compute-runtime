#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_BMG)
  set(IGDRCL_SRCS_offline_compiler_tests_xe2_hpg_core
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )
  target_sources(ocloc_tests PRIVATE ${IGDRCL_SRCS_offline_compiler_tests_xe2_hpg_core})
  add_subdirectories()
endif()
