#
# Copyright (C) 2023-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_tests_base.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_tests_base.h

               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_12_55_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_12_56_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_12_57_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_12_60_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_12_61_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_12_70_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_12_71_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_12_74_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_20_01_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_20_02_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_20_04_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_30_00_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_30_01_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_30_03_tests.cpp
)

add_subdirectories()
