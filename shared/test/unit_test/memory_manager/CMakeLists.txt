#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/multi_graphics_allocation_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/special_heap_pool_tests.cpp
)
