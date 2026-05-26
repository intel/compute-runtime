#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_OS_INTERFACE_WINDOWS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/api_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d10_11_sharing_functions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d9_sharing_functions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d_sharing_functions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device_caps_init_win.cpp
)

if(WIN32)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE
                 ${LEO_OS_INTERFACE_WINDOWS_SRCS}
  )
endif()

add_subdirectories()

set_property(GLOBAL PROPERTY LEO_OS_INTERFACE_WINDOWS_SRCS ${LEO_OS_INTERFACE_WINDOWS_SRCS})
