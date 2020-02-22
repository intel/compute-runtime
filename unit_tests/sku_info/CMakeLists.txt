#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_sku_info
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/sku_info_base_reference.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/sku_info_transfer_tests.cpp
)

if(WIN32)
  list(APPEND IGDRCL_SRCS_tests_sku_info
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/sku_info_receiver_tests.cpp
  )
endif()
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_sku_info})