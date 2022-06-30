#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_RKL)
  set(HW_SOURCES_GEN12LP
      ${HW_SOURCES_GEN12LP}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_rkl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_rkl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel_rkl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/image_rkl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/sampler_rkl.cpp
      PARENT_SCOPE
  )
endif()
