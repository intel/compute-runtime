#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_performance_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_memory_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_temperature_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_globalops_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_frequency_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_ras_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_power_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_power_tests_xe.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_pmt_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_device_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_pci_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_init_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_engine_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_engine_tests_xe.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_vf_management_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_ecc_tests.cpp
  )
endif()
add_subdirectories()
