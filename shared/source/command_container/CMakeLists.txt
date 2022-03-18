#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMMAND_CONTAINER
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdcontainer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdcontainer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_bdw_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tgllp_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/encode_compute_mode_bdw_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/encode_compute_mode_tgllp_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions/encode_surface_state_args_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}encode_surface_state.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}encode_surface_state_args.h
)

if(SUPPORT_XEHP_AND_LATER)
  list(APPEND NEO_CORE_COMMAND_CONTAINER
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_raytracing_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/walker_partition_interface.h
       ${CMAKE_CURRENT_SOURCE_DIR}/walker_partition_xehp_and_later.h
  )
endif()

if(SUPPORT_DG2_AND_LATER)
  list(APPEND NEO_CORE_COMMAND_CONTAINER
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_xe_hpg_core_and_later.inl
  )
endif()

if(SUPPORT_PVC_AND_LATER)
  list(APPEND NEO_CORE_COMMAND_CONTAINER
       ${CMAKE_CURRENT_SOURCE_DIR}/memory_fence_encoder.h
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_COMMAND_CONTAINER ${NEO_CORE_COMMAND_CONTAINER})

add_subdirectories()
