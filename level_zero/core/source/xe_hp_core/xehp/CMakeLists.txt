#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_XE_HP_SDV)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_xehp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_xehp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_xehp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_xehp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_xehp.cpp
  )
endif()
