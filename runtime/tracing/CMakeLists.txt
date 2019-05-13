#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_TRACING
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/tracing_api.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tracing_api.h
  ${CMAKE_CURRENT_SOURCE_DIR}/tracing_handle.h
  ${CMAKE_CURRENT_SOURCE_DIR}/tracing_notify.h
  ${CMAKE_CURRENT_SOURCE_DIR}/tracing_types.h
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_TRACING})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_TRACING ${RUNTIME_SRCS_TRACING})
