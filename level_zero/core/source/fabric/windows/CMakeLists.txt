#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(L0_SRCS_FABRIC_OS
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_iaf.h
      ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_iaf.cpp
      PARENT_SCOPE
  )
endif()