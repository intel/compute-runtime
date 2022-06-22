#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_DEBUGGER_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0_linux.cpp
)

if(UNIX)
  set_property(GLOBAL PROPERTY L0_SRCS_DEBUGGER_LINUX ${L0_SRCS_DEBUGGER_LINUX})
endif()
