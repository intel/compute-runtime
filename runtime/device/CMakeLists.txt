#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_DEVICE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/device.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_caps.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_info_map.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_vector.h
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_info.h
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_DEVICE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_DEVICE ${RUNTIME_SRCS_DEVICE})
