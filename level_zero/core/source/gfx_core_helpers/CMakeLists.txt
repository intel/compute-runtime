#
# Copyright (C) 2023-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper_base.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper.h
)

if(SUPPORT_XEHP_AND_LATER)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper_xehp_and_later.inl
  )
endif()

if(SUPPORT_GEN12LP OR SUPPORT_XE_HPG_CORE)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper_tgllp_to_dg2.inl
  )
endif()

if(SUPPORT_XE_HPC_CORE OR SUPPORT_XE_HPG_CORE)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper_xe_hpg_and_xe_hpc.inl
  )
endif()

if(SUPPORT_PVC_AND_LATER)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper_pvc_and_later.inl
  )
endif()

if(SUPPORT_GEN12LP OR SUPPORT_XE_HPG_CORE OR SUPPORT_XE_HPC_CORE)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper_tgllp_to_pvc.inl
  )
endif()

if(SUPPORT_XE_HPG_CORE OR SUPPORT_XE2_HPG_CORE)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper_xe_hpg_to_xe2_hpg.inl
  )
endif()

if(SUPPORT_XE2_AND_LATER)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper_xe2_hpg_and_later.inl
  )
endif()
if(SUPPORT_XE3_AND_LATER)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper_xe3_and_later.inl
  )
endif()
