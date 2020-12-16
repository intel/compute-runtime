#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/host_pointer_manager_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_driver.cpp
)

add_subdirectories()
