#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_XE_HPC_CORE)
  set(HW_DEFINITIONS_XE_HPC_CORE
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}hw_cmds_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}os_agnostic_hw_info_config_pvc_extra.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}device_ids_configs_pvc.h
      ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}hw_cmds_pvc.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/compiler_hw_info_config_pvc.inl
  )

  set_property(GLOBAL APPEND PROPERTY NEO_CORE_HELPERS ${HW_DEFINITIONS_XE_HPC_CORE})

  add_subdirectories()
endif()
