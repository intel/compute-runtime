#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/os_sysman.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_device_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_device_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_device.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_device.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_hw_device_id.h
)

add_subdirectories()
