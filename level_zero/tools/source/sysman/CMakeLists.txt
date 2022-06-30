#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_imp.h
)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_TOOLS_SYSMAN}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

add_subdirectories()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN ${L0_SRCS_TOOLS_SYSMAN})
