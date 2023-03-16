#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_PVC)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_pvc.cpp
  )
endif()
