#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_KERNEL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_info_cl.h
)
target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_KERNEL_SRCS})
set_property(GLOBAL PROPERTY LEO_KERNEL_SRCS ${LEO_KERNEL_SRCS})
