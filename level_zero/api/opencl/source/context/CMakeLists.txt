#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_CONTEXT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/context.h
)

target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_CONTEXT_SRCS})
set_property(GLOBAL PROPERTY LEO_CONTEXT_SRCS ${LEO_CONTEXT_SRCS})
