#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_FABRIC
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/fabric.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fabric_device_mdfi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fabric.cpp
)

add_subdirectories()
set(L0_RUNTIME_SOURCES
    ${L0_RUNTIME_SOURCES}
    ${L0_SRCS_FABRIC}
    ${L0_SRCS_FABRIC_OS}
    PARENT_SCOPE
)
