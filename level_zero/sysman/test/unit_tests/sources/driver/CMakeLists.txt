#
# Copyright (C) 2025-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_driver.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_driver_handle.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_hw_device_id.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_os_driver.h
               ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_driver.cpp
)
