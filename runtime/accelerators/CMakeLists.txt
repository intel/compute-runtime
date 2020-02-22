#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_ACCELERATORS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/intel_accelerator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/intel_accelerator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/intel_motion_estimation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/intel_motion_estimation.h
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_ACCELERATORS})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_ACCELERATORS ${RUNTIME_SRCS_ACCELERATORS})
