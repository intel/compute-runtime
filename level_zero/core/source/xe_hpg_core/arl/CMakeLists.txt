#
# Copyright (C) 2023-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_ARL)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_arl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_arl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_arl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_arl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_arl.cpp
  )
  add_subdirectories()
endif()
