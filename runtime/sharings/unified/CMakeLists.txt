#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_SHARINGS_UNIFIED
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/enable_unified.h
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_buffer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_buffer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_sharing.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_sharing.h
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_sharing_types.h
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_SHARINGS_UNIFIED})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_SHARINGS_UNIFIED ${RUNTIME_SRCS_SHARINGS_UNIFIED})
add_subdirectories()

target_sources(${SHARINGS_ENABLE_LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/unified_enable_name.cpp)
