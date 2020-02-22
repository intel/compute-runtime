#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_AUB_MEM_DUMP
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_alloc_dump.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_alloc_dump.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_data.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_header.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_services.h
  ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/context_flags.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/page_table_entry_bits.h
)

if(NOT DEFINED AUB_STREAM_DIR)
  list(APPEND RUNTIME_SRCS_AUB_MEM_DUMP
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_stream_stubs.cpp
  )
endif()

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_AUB_MEM_DUMP})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_AUB_MEM_DUMP ${RUNTIME_SRCS_AUB_MEM_DUMP})
add_subdirectories()
