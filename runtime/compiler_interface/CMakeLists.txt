#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_COMPILER_INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/binary_cache.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/binary_cache.h
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler_options.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler_options.h
  ${CMAKE_CURRENT_SOURCE_DIR}/compiler_interface.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/create_main.cpp
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_COMPILER_INTERFACE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_COMPILER_INTERFACE ${RUNTIME_SRCS_COMPILER_INTERFACE})
