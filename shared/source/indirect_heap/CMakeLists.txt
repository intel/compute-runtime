#
# Copyright (C) 2019-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_INDIRECT_HEAP
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/heap_size.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/heap_size.h
    ${CMAKE_CURRENT_SOURCE_DIR}/indirect_heap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/indirect_heap_type.h
)

set_property(GLOBAL PROPERTY NEO_CORE_INDIRECT_HEAP ${NEO_CORE_INDIRECT_HEAP})
add_subdirectories()
