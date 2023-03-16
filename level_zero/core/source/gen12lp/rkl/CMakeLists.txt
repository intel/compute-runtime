#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_RKL)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_rkl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_rkl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_rkl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_rkl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_rkl.cpp
  )
endif()
