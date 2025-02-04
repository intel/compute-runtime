#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_IMAGE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state.h
    ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state.inl
)

if(SUPPORT_GEN12LP OR SUPPORT_XE_HPG_CORE OR SUPPORT_XE_HPC_CORE)
  list(APPEND NEO_CORE_IMAGE
       ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_before_xe2.inl
  )
endif()

if(SUPPORT_XE2_AND_LATER)
  list(APPEND NEO_CORE_IMAGE
       ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_xe2_and_later.inl
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_IMAGE ${NEO_CORE_IMAGE})
add_subdirectories()
