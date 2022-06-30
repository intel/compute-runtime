#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_FABRICPORT
    ${CMAKE_CURRENT_SOURCE_DIR}/fabric_port.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fabric_port.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fabric_port_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fabric_port_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_fabric_port.h
)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_TOOLS_SYSMAN_FABRICPORT}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

add_subdirectories()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_FABRICPORT ${L0_SRCS_TOOLS_SYSMAN_FABRICPORT})
