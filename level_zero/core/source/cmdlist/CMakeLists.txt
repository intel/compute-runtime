#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist.h
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_extended${BRANCH_DIR_SUFFIX}cmdlist_extended.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw.h
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw_skl_to_tgllp.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw_immediate.h
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw_immediate.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_launch_params.h
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_memory_copy_params.h
               ${CMAKE_CURRENT_SOURCE_DIR}/command_to_patch.h
               ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}cmdlist_launch_params_ext.h
               ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}copy_offload_mode.h
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}cmdlist_additional_args.cpp
)

if(SUPPORT_GEN12LP OR SUPPORT_XE_HPG_CORE OR SUPPORT_XE_HPC_CORE OR SUPPORT_XE2_HPG_CORE OR SUPPORT_XE3_CORE)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw_gen12lp_to_xe3.inl
  )
endif()

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
