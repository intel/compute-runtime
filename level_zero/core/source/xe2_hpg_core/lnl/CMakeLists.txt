#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_LNL)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_lnl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_lnl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_lnl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_lnl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_lnl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_lnl.cpp
  )
  add_subdirectories()
endif()
