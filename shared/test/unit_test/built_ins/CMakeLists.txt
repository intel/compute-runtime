#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/builtin_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sip_tests.cpp
)

add_subdirectories()
