#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_GEN12LP)
  set(HW_SOURCES_GEN12LP
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_gen12lp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_gen12lp.h
      ${CMAKE_CURRENT_SOURCE_DIR}/debugger_gen12lp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/definitions/cache_flush_gen12lp.inl
      ${CMAKE_CURRENT_SOURCE_DIR}/enable_family_full_l0_gen12lp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/l0_hw_helper_gen12lp.cpp
  )
  add_subdirectories()
  target_include_directories(${L0_STATIC_LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/definitions/)

  target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${HW_SOURCES_GEN12LP})
  set_property(GLOBAL APPEND PROPERTY L0_HW_SOURCES_GEN12LP ${HW_SOURCES_GEN12LP})
endif()
