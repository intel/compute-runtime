#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_libult_common PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/multicontext_aub_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/multicontext_aub_fixture.h
)

add_subdirectories()
