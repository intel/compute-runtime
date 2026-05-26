#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_COMMAND_QUEUE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/command_queue.cpp
)

target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_COMMAND_QUEUE_SRCS})
set_property(GLOBAL PROPERTY LEO_COMMAND_QUEUE_SRCS ${LEO_COMMAND_QUEUE_SRCS})
add_subdirectories()
