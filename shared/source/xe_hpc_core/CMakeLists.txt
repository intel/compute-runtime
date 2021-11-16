#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_XE_HPC_CORE)
  set(HW_DEFINITIONS_XE_HPC_CORE
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}hw_cmds_pvc.inl
  )
  set_property(GLOBAL APPEND PROPERTY CORE_SRCS_GENX_ALL_BASE ${HW_DEFINITIONS_XE_HPC_CORE})

  add_subdirectories()
endif()
