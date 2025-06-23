#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_memory.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_memory_pooling.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_memory_reuse.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_memory_${DRIVER_MODEL}.cpp
)
add_subdirectories()
