#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_ADLN)
  set(HW_SOURCES_GEN12LP
      ${HW_SOURCES_GEN12LP}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_adln.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_adln.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel_adln.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/image_adln.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/sampler_adln.cpp
      PARENT_SCOPE
  )
endif()
