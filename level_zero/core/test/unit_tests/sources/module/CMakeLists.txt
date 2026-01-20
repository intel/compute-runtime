#
# Copyright (C) 2020-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_module.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_module_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_modules_package.cpp
)

add_subdirectories()
