#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/stream_properties_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/stream_properties_tests_common.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/stream_properties_tests_common.h
)
add_subdirectories()
