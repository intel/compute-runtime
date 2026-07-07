#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_SHARINGS_UNIFIED_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_enable_unified.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_enable_unified.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_unified_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_unified_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_unified_image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_unified_image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_unified_sharing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_unified_sharing.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_unified_sharing_types.h
)

target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_SHARINGS_UNIFIED_SRCS})
set_property(GLOBAL PROPERTY LEO_SHARINGS_UNIFIED_SRCS ${LEO_SHARINGS_UNIFIED_SRCS})
add_subdirectories()
