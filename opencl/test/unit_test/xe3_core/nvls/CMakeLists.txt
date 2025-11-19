#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_NVLS)
  set(IGDRCL_SRCS_tests_xe3_core_nvls
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe3_core_nvls})
  add_subdirectories()
endif()
