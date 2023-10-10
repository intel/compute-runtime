#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ARL)
  set(IGDRCL_SRCS_tests_xe_hpg_core_arl
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe_hpg_core_arl})
  add_subdirectories()
endif()
