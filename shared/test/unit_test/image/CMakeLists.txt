#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests.cpp
)

add_subdirectories()
