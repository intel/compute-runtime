#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMPILER_RELEASE_HELPER
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_release_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_release_helper.cpp
)

set(NEO_CORE_ENABLE_COMPILER_RELEASE_HELPER
    ${CMAKE_CURRENT_SOURCE_DIR}/enable_compiler_release_helper_xe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/enable_compiler_release_helper_xe2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/enable_compiler_release_helper_xe3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}enable_compiler_release_helper_xe3p.cpp
)

set_property(GLOBAL APPEND PROPERTY NEO_CORE_ENABLE_COMPILER_RELEASE_HELPER ${NEO_CORE_ENABLE_COMPILER_RELEASE_HELPER})

set_property(GLOBAL APPEND PROPERTY NEO_CORE_COMPILER_RELEASE_HELPER ${NEO_CORE_COMPILER_RELEASE_HELPER})

add_subdirectories()
