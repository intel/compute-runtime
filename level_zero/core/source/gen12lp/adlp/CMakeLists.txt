#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_ADLP)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_adlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_adlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_adlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_adlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_adlp.cpp
  )
endif()
