#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_ADLS)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_adls.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_adls.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_adls.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_adls.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_adls.cpp
  )
endif()
