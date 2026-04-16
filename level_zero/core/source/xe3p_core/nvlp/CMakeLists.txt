#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_NVLP)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_nvlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_nvlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_nvlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_nvlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_nvlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_nvlp.cpp
  )
endif()
