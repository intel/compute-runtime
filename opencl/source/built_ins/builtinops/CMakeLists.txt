#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_BUILT_INS_OPS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/built_in_ops.h
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_EMBARGO_BUILT_INS_OPS})

add_subdirectories()