#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_GEN_COMMON
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_mapper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_mapper_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_cmds.h
  ${CMAKE_CURRENT_SOURCE_DIR}/reg_configs${BRANCH_DIR_SUFFIX}/reg_configs_common.h
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_GEN_COMMON})
