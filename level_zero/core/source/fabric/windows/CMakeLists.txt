#
# Copyright (C) 2022-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/fabric.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_iaf.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_iaf.cpp
  )
endif()
