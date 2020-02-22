#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_SKU_INFO_BASE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/sku_info_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/operations/sku_info_transfer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}/sku_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/operations${BRANCH_DIR_SUFFIX}/sku_info_transfer.cpp
)

set(NEO_CORE_SKU_INFO_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/operations/windows/sku_info_receiver.h
  ${CMAKE_CURRENT_SOURCE_DIR}/operations/windows${BRANCH_DIR_SUFFIX}/sku_info_receiver.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_SKU_INFO_BASE ${NEO_CORE_SKU_INFO_BASE})
set_property(GLOBAL PROPERTY NEO_CORE_SKU_INFO_WINDOWS ${NEO_CORE_SKU_INFO_WINDOWS})
