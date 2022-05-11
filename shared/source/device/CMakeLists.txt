#
# Copyright (C) 2020-2022 Intel Corporation
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
)
if(WIN32)
  list(APPEND NEO_CORE_DEVICE
       ${CMAKE_CURRENT_SOURCE_DIR}/windows/device_windows.cpp
  )
else()
  list(APPEND NEO_CORE_DEVICE
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/device_linux.cpp
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_DEVICE ${NEO_CORE_DEVICE})
add_subdirectories()
