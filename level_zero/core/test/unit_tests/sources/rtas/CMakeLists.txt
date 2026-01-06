#
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_rtas.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_rtas_ext.cpp
)
add_subdirectories()