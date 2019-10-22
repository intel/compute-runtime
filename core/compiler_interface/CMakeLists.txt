#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_COMPILER_INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler_cache.h
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler_cache.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler_interface.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/create_main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linker.h
  ${CMAKE_CURRENT_SOURCE_DIR}/linker.cpp
)

set_property(GLOBAL PROPERTY NEO_COMPILER_INTERFACE ${NEO_COMPILER_INTERFACE})
