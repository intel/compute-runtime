#
# Copyright (C) 2020-2023 Intel Corporation
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
  )
endif()

