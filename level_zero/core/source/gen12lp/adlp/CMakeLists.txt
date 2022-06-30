#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_ADLP)
  set(HW_SOURCES_GEN12LP
      ${HW_SOURCES_GEN12LP}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_adlp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_adlp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel_adlp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/image_adlp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/sampler_adlp.cpp
      PARENT_SCOPE
  )
endif()
