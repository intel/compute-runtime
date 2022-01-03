#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_DEVICE_QUEUE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/device_queue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_queue.h
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_DEVICE_QUEUE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_DEVICE_QUEUE ${RUNTIME_SRCS_DEVICE_QUEUE})
add_subdirectories()
