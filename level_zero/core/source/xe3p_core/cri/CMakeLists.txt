#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_CRI)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_cri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_cri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_cri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_cri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_cri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_cri.cpp
  )
endif()
