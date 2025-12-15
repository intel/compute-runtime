#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_AUB_MEM_DUMP
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_alloc_dump.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_alloc_dump.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/page_table_entry_bits.h
)

set_property(GLOBAL PROPERTY NEO_CORE_AUB_MEM_DUMP ${NEO_CORE_AUB_MEM_DUMP})
add_subdirectories()
