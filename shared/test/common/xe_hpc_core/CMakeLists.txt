#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPC_CORE)
  set(NEO_CORE_TESTS_XE_HPC_CORE
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}unit_test_helper_xe_hpc_core_extra.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_xe_hpc_core.h
  )

  set_property(GLOBAL PROPERTY NEO_CORE_TESTS_XE_HPC_CORE ${NEO_CORE_TESTS_XE_HPC_CORE})

  add_subdirectories()

  target_sources(neo_libult_common PRIVATE ${NEO_CORE_TESTS_XE_HPC_CORE})
endif()