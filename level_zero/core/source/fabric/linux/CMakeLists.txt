#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)

  if(LIBGENL_FOUND AND NEO_ENABLE_i915_PRELIM_DETECTION)
    set(L0_SRCS_FABRIC_OS
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_iaf.h
        ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_iaf.cpp
        PARENT_SCOPE
    )
  else()
    set(L0_SRCS_FABRIC_OS
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_iaf_stub.h
        ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_iaf_stub.cpp
        PARENT_SCOPE
    )
  endif()

endif()
