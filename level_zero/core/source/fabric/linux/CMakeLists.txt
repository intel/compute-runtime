#
# Copyright (C) 2022-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)

  if(LIBGENL_FOUND AND NEO_ENABLE_I915_PRELIM_DETECTION)
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/fabric.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_iaf.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_iaf.cpp
    )
  else()
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/fabric.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_iaf_stub.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_iaf_stub.cpp
    )
  endif()

endif()
