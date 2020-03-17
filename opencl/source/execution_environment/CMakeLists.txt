#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_EXECUTION_ENVIRONMENT
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/cl_execution_environment.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cl_execution_environment.cpp
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_EXECUTION_ENVIRONMENT})