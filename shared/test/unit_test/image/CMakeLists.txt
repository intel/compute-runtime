#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests.cpp
)

add_subdirectories()
