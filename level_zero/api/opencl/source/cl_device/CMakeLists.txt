#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_DEVICE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device_caps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device_get_cap.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device_info_map.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}cl_device_info_extra.cpp
)
target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_DEVICE_SRCS})
set_property(GLOBAL PROPERTY LEO_DEVICE_SRCS ${LEO_DEVICE_SRCS})

add_subdirectories()
