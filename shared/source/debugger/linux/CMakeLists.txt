#
# Copyright (C) 2020-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_SRCS_DEBUGGER_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger_xe.h
)

set_property(GLOBAL PROPERTY NEO_CORE_SRCS_DEBUGGER_LINUX ${NEO_CORE_SRCS_DEBUGGER_LINUX})