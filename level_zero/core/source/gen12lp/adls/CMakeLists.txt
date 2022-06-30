#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_ADLS)
  set(HW_SOURCES_GEN12LP
      ${HW_SOURCES_GEN12LP}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_adls.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_adls.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel_adls.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/image_adls.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/sampler_adls.cpp
      PARENT_SCOPE
  )
endif()
