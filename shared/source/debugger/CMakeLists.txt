#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_DEBUGGER
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0.h
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0_tgllp_and_later.inl
)

set_property(GLOBAL PROPERTY NEO_CORE_DEBUGGER ${NEO_CORE_DEBUGGER})

add_subdirectories()

