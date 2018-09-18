#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_SKU_INFO_BASE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/sku_info_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/operations/sku_info_transfer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}/sku_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/operations${BRANCH_DIR_SUFFIX}/sku_info_transfer.cpp
)

set(RUNTIME_SRCS_SKU_INFO_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/operations/sku_info_receiver.h
  ${CMAKE_CURRENT_SOURCE_DIR}/operations${BRANCH_DIR_SUFFIX}/sku_info_receiver.cpp
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_SKU_INFO_BASE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_SKU_INFO_BASE ${RUNTIME_SRCS_SKU_INFO_BASE})

if(WIN32)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_SKU_INFO_WINDOWS})
endif()
