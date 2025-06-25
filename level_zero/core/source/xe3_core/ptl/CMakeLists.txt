#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_PTL)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_ptl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_ptl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_ptl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_ptl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_ptl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_ptl.cpp
  )
  add_subdirectories()
endif()
