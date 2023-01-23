#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_XE_HPG_CORE)
  set(HW_SOURCES_XE_HPG_CORE
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_xe_hpg_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_xe_hpg_core.h
      ${CMAKE_CURRENT_SOURCE_DIR}/image_xe_hpg_core.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/sampler_xe_hpg_core.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/enable_family_full_l0_xe_hpg_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper_xe_hpg_core.cpp
  )

  add_subdirectories()

  target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${HW_SOURCES_XE_HPG_CORE})
  set_property(GLOBAL PROPERTY L0_HW_SOURCES_XE_HPG_CORE ${HW_SOURCES_XE_HPG_CORE})
endif()
