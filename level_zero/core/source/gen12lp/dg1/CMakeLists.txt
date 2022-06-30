#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_DG1)
  set(HW_SOURCES_GEN12LP
      ${HW_SOURCES_GEN12LP}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_dg1.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_dg1.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel_dg1.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/image_dg1.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/sampler_dg1.cpp
      PARENT_SCOPE
  )
endif()
