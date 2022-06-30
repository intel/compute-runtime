#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_OS_INTERFACE_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/api_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d_sharing_functions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device_caps_init_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ocl_reg_path.cpp
)

if(UNIX)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_OS_INTERFACE_LINUX})
endif()

set_property(GLOBAL PROPERTY RUNTIME_SRCS_OS_INTERFACE_LINUX ${RUNTIME_SRCS_OS_INTERFACE_LINUX})
add_subdirectories()
