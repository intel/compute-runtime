#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_L0_DISPATCH_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_l0_dispatch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_l0_dispatch.h
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_l0_dispatch_entries.inl
)

target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_L0_DISPATCH_SRCS})
set_property(GLOBAL PROPERTY LEO_L0_DISPATCH_SRCS ${LEO_L0_DISPATCH_SRCS})
