#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_SHARINGS_VA_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_enable_va.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_enable_va.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_va_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_va_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_va_device_shared.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}leo_va_extension.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_cl_va_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_cl_va_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_va_sharing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_va_sharing.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_va_sharing_defines.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_va_sharing_functions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_va_sharing_functions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_va_surface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_va_surface.h
)
set_property(GLOBAL PROPERTY LEO_SHARINGS_VA_SRCS ${LEO_SHARINGS_VA_SRCS})

if(NEO__LIBVA_FOUND)
  target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_SHARINGS_VA_SRCS})
endif()

add_subdirectories()
