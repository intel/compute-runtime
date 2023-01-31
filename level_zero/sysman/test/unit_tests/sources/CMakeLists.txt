#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_driver.h
               ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_driver.cpp
)

add_subdirectories()
