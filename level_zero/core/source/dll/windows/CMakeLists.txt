#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_DLL_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${NEO_SOURCE_DIR}/level_zero/tools/source/debug/windows/debug_session_windows_helper.cpp
)

set_property(GLOBAL PROPERTY L0_SRCS_DLL_WINDOWS ${L0_SRCS_DLL_WINDOWS})
