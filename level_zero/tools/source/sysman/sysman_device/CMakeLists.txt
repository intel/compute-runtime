#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_DEVICE
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_device_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_device_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_sysman_device.h
)

target_include_directories(${TARGET_NAME_L0}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

target_sources(${TARGET_NAME_L0}
    PRIVATE
    ${L0_SRCS_TOOLS_SYSMAN_DEVICE}
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(UNIX)
    add_subdirectory(linux)
else()
    add_subdirectory(windows)
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_DEVICE ${L0_SRCS_TOOLS_SYSMAN_DEVICE})
