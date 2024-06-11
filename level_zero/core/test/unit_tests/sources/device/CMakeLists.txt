#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_l0_device.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_device_pci_bus_info.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_device_pci_bus_info.h
               ${CMAKE_CURRENT_SOURCE_DIR}/test_device_pci_speed_info.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_device_pci_speed_info.h
               ${CMAKE_CURRENT_SOURCE_DIR}/device_${DRIVER_MODEL}/test_device.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/device_${DRIVER_MODEL}/test_device_exensions.cpp
)
add_subdirectories()
