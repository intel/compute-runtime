#
# Copyright (C) 2019-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMMAND_CONTAINER
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdcontainer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdcontainer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_bdw_plus.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/encode_compute_mode_bdw_plus.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/encode_compute_mode_tgllp_plus.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling.h
)

if(SUPPORT_XEHP_PLUS)
  list(APPEND NEO_CORE_COMMAND_CONTAINER
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_raytracing_xehp_plus.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_xehp_plus.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling_xehp_plus.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/walker_partition_xehp_plus.h
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_COMMAND_CONTAINER ${NEO_CORE_COMMAND_CONTAINER})

add_subdirectories()
