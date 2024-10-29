#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_kmd_interface.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_kmd_interface_i915_prelim.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_kmd_interface_i915_upstream.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_kmd_interface_xe.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_kmd_interface.h
  )
endif()