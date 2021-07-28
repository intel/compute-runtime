#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_DEVICE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device_caps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/root_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/root_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sub_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sub_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device_get_device_name.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/device_extended_setup.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_DEVICE ${NEO_CORE_DEVICE})
add_subdirectories()
