#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_DLL_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0_linux.cpp
    ${NEO_SOURCE_DIR}/level_zero/tools/source/debug${BRANCH_DIR_SUFFIX}linux/debug_session_linux_helper.cpp
)

set_property(GLOBAL PROPERTY L0_SRCS_DLL_LINUX ${L0_SRCS_DLL_LINUX})
