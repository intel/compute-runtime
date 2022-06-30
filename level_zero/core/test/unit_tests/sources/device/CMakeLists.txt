#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_device.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_device_pci_speed_info.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_device_pci_speed_info.h
)
add_subdirectories()
