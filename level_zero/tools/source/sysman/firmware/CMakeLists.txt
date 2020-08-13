#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_FIRMWARE
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware.h
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_firmware.h
)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_TOOLS_SYSMAN_FIRMWARE}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

add_subdirectories()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_FIRMWARE ${L0_SRCS_TOOLS_SYSMAN_FIRMWARE})

