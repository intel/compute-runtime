#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_XE_HP_CORE)
  set(HW_DEFINITIONS_XE_HP_CORE
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}hw_cmds_xehp.inl
  )
  set_property(GLOBAL APPEND PROPERTY CORE_SRCS_COREX_ALL_BASE ${HW_DEFINITIONS_XE_HP_CORE})

  add_subdirectories()
endif()
