#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_FAN
    ${CMAKE_CURRENT_SOURCE_DIR}/fan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fan.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fan_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fan_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_fan.h
)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_TOOLS_SYSMAN_FAN}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

add_subdirectories()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_FAN ${L0_SRCS_TOOLS_SYSMAN_FAN})
