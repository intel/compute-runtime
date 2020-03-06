#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_DEBUGGER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0.h
)

set_property(GLOBAL PROPERTY L0_SRCS_DEBUGGER ${L0_SRCS_DEBUGGER})
