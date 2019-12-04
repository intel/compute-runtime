#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_PROGRAM
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/print_formatter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/print_formatter.h
)

set_property(GLOBAL PROPERTY NEO_CORE_PROGRAM ${NEO_CORE_PROGRAM})
