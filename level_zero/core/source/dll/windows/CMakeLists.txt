#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_DLL_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0_windows.cpp
)

set_property(GLOBAL PROPERTY L0_SRCS_DLL_WINDOWS ${L0_SRCS_DLL_WINDOWS})
