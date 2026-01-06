#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_temperature_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_memory_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_power_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_init_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_firmware_tests.cpp
  )
endif()
add_subdirectories()
