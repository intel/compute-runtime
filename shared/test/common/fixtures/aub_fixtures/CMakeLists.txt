#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_libult_common PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/multicontext_aub_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/multicontext_aub_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}multicontext_aub_fixture_extended.cpp
)

add_subdirectories()
