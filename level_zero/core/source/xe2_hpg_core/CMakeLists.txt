#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_XE2_HPG_CORE)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_xe2_hpg_core.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_xe2_hpg_core.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/enable_family_full_l0_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_xe2_hpg_core.h
  )

  add_subdirectories()

endif()
