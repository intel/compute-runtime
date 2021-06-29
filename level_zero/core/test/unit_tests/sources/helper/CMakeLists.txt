#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/api_specific_config_l0_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/heap_assigner_l0_tests.cpp
)
add_subdirectories()
