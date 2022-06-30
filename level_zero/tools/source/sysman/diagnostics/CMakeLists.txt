#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_DIAGNOSTICS
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_diagnostics.h
)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_TOOLS_SYSMAN_DIAGNOSTICS}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

add_subdirectories()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_DIAGNOSTICS ${L0_SRCS_TOOLS_SYSMAN_DIAGNOSTICS})

