#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_ICLLP)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_icllp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_icllp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/kernel_icllp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_icllp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_icllp.cpp
  )
endif()
