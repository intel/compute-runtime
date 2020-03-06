#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_SRCS_SOURCE_LEVEL_DEBUGGER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source_level_debugger.h
)

set_property(GLOBAL PROPERTY NEO_CORE_SRCS_SOURCE_LEVEL_DEBUGGER ${NEO_CORE_SRCS_SOURCE_LEVEL_DEBUGGER})
