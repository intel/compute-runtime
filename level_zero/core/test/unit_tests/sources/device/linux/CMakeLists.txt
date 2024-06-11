#
# Copyright (C) 2021-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_uuid.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_pci_bus_info_linux.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_pci_speed_info_linux.cpp
  )
endif()
