#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_imp.h
)

target_include_directories(${TARGET_NAME_L0}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

target_sources(${TARGET_NAME_L0}
    PRIVATE
        ${L0_SRCS_TOOLS_SYSMAN}
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

    add_subdirectory(sysman_device)
    add_subdirectory(pci)
    add_subdirectory(frequency)
    add_subdirectory(standby)

if(UNIX)
    add_subdirectory(linux)
else()
    add_subdirectory(windows)
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN ${L0_SRCS_TOOLS_SYSMAN})
