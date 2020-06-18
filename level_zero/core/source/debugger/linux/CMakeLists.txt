#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_DEBUGGER_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0_linux.h
)

set_property(GLOBAL PROPERTY L0_SRCS_DEBUGGER_LINUX ${L0_SRCS_DEBUGGER_LINUX})
