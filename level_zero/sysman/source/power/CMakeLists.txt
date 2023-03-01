#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_SYSMAN_POWER
    ${CMAKE_CURRENT_SOURCE_DIR}/power.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/power.h
    ${CMAKE_CURRENT_SOURCE_DIR}/power_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/power_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_power.h
)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_SYSMAN_POWER}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

add_subdirectories()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_SYSMAN_POWER ${L0_SRCS_SYSMAN_POWER})

