#
# Copyright (C) 2022-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_ADLN)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_adln.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_adln.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_adln.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_adln.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_adln.cpp
  )
endif()
