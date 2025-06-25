#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_DG2)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_dg2.cpp
  )
  add_subdirectories()
endif()
