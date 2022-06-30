#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_PVC)
  set(HW_SOURCES_XE_HPC_CORE
      ${HW_SOURCES_XE_HPC_CORE}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/image_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel_pvc.cpp
      PARENT_SCOPE
  )
endif()
