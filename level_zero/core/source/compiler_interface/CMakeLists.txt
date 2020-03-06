#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_COMPILER_INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/default_cache_config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/default_l0_cache_config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/default_l0_cache_config.h
  ${CMAKE_CURRENT_SOURCE_DIR}/l0_reg_path.h
)

set_property(GLOBAL PROPERTY L0_SRCS_COMPILER_INTERFACE ${L0_SRCS_COMPILER_INTERFACE})
