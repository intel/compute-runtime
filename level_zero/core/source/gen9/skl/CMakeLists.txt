#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_SKL)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_skl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_skl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_skl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_skl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_skl.cpp
  )
endif()
