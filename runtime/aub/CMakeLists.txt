#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_AUB
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_center.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_center.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/aub_helper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper_base.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper_bdw_plus.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper_add_mmio.cpp
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_AUB})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_AUB ${RUNTIME_SRCS_AUB})
add_subdirectories()
