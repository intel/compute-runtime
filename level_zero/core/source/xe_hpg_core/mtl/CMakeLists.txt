#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_MTL)
  set(HW_SOURCES_XE_HPG_CORE
      ${HW_SOURCES_XE_HPG_CORE}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_mtl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_mtl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel_mtl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/image_mtl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/sampler_mtl.cpp
      PARENT_SCOPE
  )
endif()
