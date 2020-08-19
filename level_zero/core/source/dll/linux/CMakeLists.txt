#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_DLL_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0_linux.cpp
)

set_property(GLOBAL PROPERTY L0_SRCS_DLL_LINUX ${L0_SRCS_DLL_LINUX})
