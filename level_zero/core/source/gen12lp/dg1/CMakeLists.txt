#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_DG1)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_dg1.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_dg1.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_dg1.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_dg1.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_dg1.cpp
  )
endif()
