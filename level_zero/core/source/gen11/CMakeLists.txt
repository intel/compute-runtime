#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_GEN11)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_gen11.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/enable_family_full_l0_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper_gen11.cpp
  )
  add_subdirectories()

endif()
