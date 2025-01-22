#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMMAND_CONTAINER
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdcontainer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdcontainer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_enablers.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tgllp_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/encode_alu_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/encode_surface_state.h
    ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling.h
    ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling_before_xe_hp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions/encode_size_preferred_slm_value.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions/encode_surface_state_args_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}encode_surface_state.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}encode_surface_state_args.h
)

if(SUPPORT_XEHP_AND_LATER)
  list(APPEND NEO_CORE_COMMAND_CONTAINER
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/walker_partition_interface.h
       ${CMAKE_CURRENT_SOURCE_DIR}/walker_partition_xehp_and_later.h
  )
endif()

if(SUPPORT_XE_HPG OR SUPPORT_XE_HPC)
  list(APPEND NEO_CORE_COMMAND_CONTAINER
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_xe_hpg_core_and_xe_hpc.inl
  )
endif()

if(SUPPORT_XE_HPG OR SUPPORT_XE_HPC OR SUPPORT_XE2_HPG_CORE)
  list(APPEND NEO_CORE_COMMAND_CONTAINER
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_from_xe_hpg_core_to_xe2_hpg.inl
  )
endif()

if(SUPPORT_XE_HPG OR SUPPORT_XE_HPC OR SUPPORT_XE2_HPG_CORE OR SUPPORT_XE3_CORE)
  list(APPEND NEO_CORE_COMMAND_CONTAINER
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_from_xe_hpg_core_to_xe3_core.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/walker_partition_from_xe_hpg_core_to_xe3_core.inl
  )
endif()

if(SUPPORT_GEN12LP OR SUPPORT_XE_HPG)
  list(APPEND NEO_CORE_COMMAND_CONTAINER
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_gen12lp_and_xe_hpg.inl
  )
endif()

if(SUPPORT_PVC_AND_LATER)
  list(APPEND NEO_CORE_COMMAND_CONTAINER
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_xe_hpc_core_and_later.inl
  )
endif()

if(SUPPORT_XE2_AND_LATER)
  list(APPEND NEO_CORE_COMMAND_CONTAINER
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_xe2_hpg_core_and_later.inl
  )
endif()

if(SUPPORT_XE3_AND_LATER)
  list(APPEND NEO_CORE_COMMAND_CONTAINER
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_from_xe3_and_later.inl
  )
endif()

if(SUPPORT_XE_HPC_AND_BEFORE)
  list(APPEND NEO_CORE_COMMAND_CONTAINER
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_pre_xe2_hpg_core.inl
  )
endif()

if(SUPPORT_XE_HPC_AND_BEFORE OR SUPPORT_XE2_HPG_CORE)
  list(APPEND NEO_CORE_COMMAND_CONTAINER
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_from_gen12lp_to_xe2_hpg.inl
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_COMMAND_CONTAINER ${NEO_CORE_COMMAND_CONTAINER})

add_subdirectories()
