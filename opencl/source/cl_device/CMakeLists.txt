#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_CL_DEVICE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device_caps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device_get_cap.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device_info_map.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_device_vector.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}cl_device_info_extra.cpp
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_CL_DEVICE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_CL_DEVICE ${RUNTIME_SRCS_CL_DEVICE})

add_subdirectories()
