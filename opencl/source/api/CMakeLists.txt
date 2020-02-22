#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_API
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/additional_extensions.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/additional_extensions.h
  ${CMAKE_CURRENT_SOURCE_DIR}/api.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api.h
  ${CMAKE_CURRENT_SOURCE_DIR}/api_enter.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cl_types.h
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch.h
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_API})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_API ${RUNTIME_SRCS_API})
add_subdirectories()
