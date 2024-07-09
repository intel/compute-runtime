#
# Copyright (C) 2021-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist.h
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw.h
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw_skl_to_tgllp.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw_immediate.h
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw_immediate.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_launch_params.h
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_extended${BRANCH_DIR_SUFFIX}cmdlist_extended.inl
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}mcl_cmdlist.h
)

if(SUPPORT_XEHP_AND_LATER)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw_xehp_and_later.inl
  )
endif()

if(SUPPORT_XE_HPG_CORE OR SUPPORT_XE_HPC_CORE)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw_dg2_and_pvc.inl
  )
endif()

if(SUPPORT_PVC_AND_LATER)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw_xe_hpc_and_later.inl
  )
endif()

if(SUPPORT_XE2_AND_LATER)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw_xe2_hpg_and_later.inl
  )
endif()

add_subdirectories()
