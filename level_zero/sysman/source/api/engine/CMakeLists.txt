#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_SYSMAN_ENGINE
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_engine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_engine_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_engine_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_engine.h
)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_SYSMAN_ENGINE}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

add_subdirectories()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_SYSMAN_ENGINE ${L0_SRCS_SYSMAN_ENGINE})
