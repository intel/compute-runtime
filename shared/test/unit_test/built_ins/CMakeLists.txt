#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/built_in_tests_shared.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/sip_tests.cpp
)

add_subdirectories()
