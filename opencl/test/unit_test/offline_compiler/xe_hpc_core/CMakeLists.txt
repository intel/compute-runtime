#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPC_CORE)
  set(IGDRCL_SRCS_offline_compiler_tests_xe_hpc_core
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )
  target_sources(ocloc_tests PRIVATE ${IGDRCL_SRCS_offline_compiler_tests_xe_hpc_core})
  add_subdirectories()
endif()
