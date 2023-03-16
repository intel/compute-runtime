#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_CFL)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_cfl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_cfl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_cfl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_cfl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_cfl.cpp
  )
endif()
