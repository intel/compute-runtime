#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPG_CORE)
  set(IGDRCL_SRCS_tests_program_xe_hpg_core
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_program_xe_hpg_core})
  add_subdirectories()
endif()
