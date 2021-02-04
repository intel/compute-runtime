#
# Copyright (C) 2019-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/command_container_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests.cpp
)
add_subdirectories()
