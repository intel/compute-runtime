#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_pci_device_properties.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_pci_device_properties.cpp
  )
endif()
