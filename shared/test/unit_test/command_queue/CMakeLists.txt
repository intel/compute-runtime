#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/flattened_id_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/local_id_tests.cpp
)
add_subdirectories()
