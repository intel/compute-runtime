#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_AUB_MEM_DUMP
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_alloc_dump.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_alloc_dump.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_data.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_header.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump.inl
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}context_flags.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/page_table_entry_bits.h
)

if(NOT DEFINED AUB_STREAM_PROJECT_NAME)
  list(APPEND NEO_CORE_AUB_MEM_DUMP
       ${CMAKE_CURRENT_SOURCE_DIR}/aub_stream_stubs.cpp
  )
endif()

if(SUPPORT_XEHP_AND_LATER)
  list(APPEND NEO_CORE_AUB_MEM_DUMP
       ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump_xehp_and_later.inl
  )
endif()

if(SUPPORT_PVC_AND_LATER)
  list(APPEND NEO_CORE_AUB_MEM_DUMP
       ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump_pvc_and_later.inl
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_AUB_MEM_DUMP ${NEO_CORE_AUB_MEM_DUMP})
add_subdirectories()
