#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_BMG)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_bmg.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_bmg.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_bmg.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_bmg.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_bmg.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_bmg.cpp
  )
  add_subdirectories()
endif()
