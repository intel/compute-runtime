#
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_MTL)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_mtl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_mtl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_mtl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_mtl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_mtl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_mtl.cpp
  )
  add_subdirectories()
endif()
