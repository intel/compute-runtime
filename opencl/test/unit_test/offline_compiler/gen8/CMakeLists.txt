#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN8)
  set(IGDRCL_SRCS_offline_compiler_tests_gen8
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )
  target_sources(ocloc_tests PRIVATE ${IGDRCL_SRCS_offline_compiler_tests_gen8})

  add_subdirectories()

endif()
