#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/os_sysman_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_sysman_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysfs_access.cpp)

if(UNIX)
target_sources(${TARGET_NAME_L0}
    PRIVATE
        ${L0_SRCS_TOOLS_SYSMAN_LINUX}
)
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_LINUX ${L0_SRCS_TOOLS_SYSMAN_LINUX})