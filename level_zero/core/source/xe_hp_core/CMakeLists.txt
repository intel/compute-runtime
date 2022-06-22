#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_XE_HP_CORE)
  set(HW_SOURCES_XE_HP_CORE
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_xe_hp_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_xe_hp_core.h
      ${CMAKE_CURRENT_SOURCE_DIR}/debugger_xe_hp_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/image_xe_hp_core.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/sampler_xe_hp_core.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/enable_family_full_l0_xe_hp_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/l0_hw_helper_xe_hp_core.cpp
  )

  add_subdirectories()

  target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${HW_SOURCES_XE_HP_CORE})
  set_property(GLOBAL PROPERTY L0_HW_SOURCES_XE_HP_CORE ${HW_SOURCES_XE_HP_CORE})
endif()
