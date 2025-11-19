#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_NVLS)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_nvls.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_nvls.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_nvls.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_nvls.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_nvls.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_nvls.cpp
  )
endif()
