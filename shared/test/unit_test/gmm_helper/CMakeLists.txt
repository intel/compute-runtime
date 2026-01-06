#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/compression_tests_xe2_and_later.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/gmm_callbacks_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/gmm_helper_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/gmm_resource_info_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/gmm_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/gmm_resource_info_${DRIVER_MODEL}_tests.cpp
)

add_subdirectories()