#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_MEMORY
    ${CMAKE_CURRENT_SOURCE_DIR}/memory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_memory.h
)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_TOOLS_SYSMAN_MEMORY}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

add_subdirectories()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_MEMORY ${L0_SRCS_TOOLS_SYSMAN_MEMORY})
