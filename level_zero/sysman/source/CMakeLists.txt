#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_SYSMAN
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_driver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_driver_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_driver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_driver_handle.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_driver_handle_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_driver_handle_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_device_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_device_imp.h
)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_SYSMAN}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

add_subdirectories()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_SYSMAN ${L0_SRCS_SYSMAN})