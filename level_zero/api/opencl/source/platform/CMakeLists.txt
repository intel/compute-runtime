#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_PLATFORM_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/platform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform.h
    ${CMAKE_CURRENT_SOURCE_DIR}/platform_info.h
)
target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_PLATFORM_SRCS})
set_property(GLOBAL PROPERTY LEO_PLATFORM_SRCS ${LEO_PLATFORM_SRCS})
