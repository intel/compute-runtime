#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/api_specific_config_l0_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/api_handle_helper_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/error_code_helper_l0_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/heap_assigner_l0_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/properties_parser_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/ze_object_utils.h
               ${CMAKE_CURRENT_SOURCE_DIR}/image_helper_l0_tests.cpp
)
add_subdirectories()
