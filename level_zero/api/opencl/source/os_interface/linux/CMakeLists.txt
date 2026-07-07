#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_OS_INTERFACE_LINUX_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_api_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_d3d_sharing_functions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_device_caps_init_linux.cpp
)

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_OS_INTERFACE_LINUX_SRCS})
endif()

set_property(GLOBAL PROPERTY LEO_OS_INTERFACE_LINUX_SRCS ${LEO_OS_INTERFACE_LINUX_SRCS})
add_subdirectories()
