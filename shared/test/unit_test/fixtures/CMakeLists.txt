#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_aub_center_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/templated_fixture_tests.cpp
)

add_subdirectories()
