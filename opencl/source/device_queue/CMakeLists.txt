#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_DEVICE_QUEUE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/device_queue.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_queue.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_queue_hw.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_queue_hw_base.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/device_queue_hw_bdw_plus.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/device_queue_hw_profiling.inl
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_DEVICE_QUEUE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_DEVICE_QUEUE ${RUNTIME_SRCS_DEVICE_QUEUE})
add_subdirectories()
