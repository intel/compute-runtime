#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/gmm_helper_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/gmm_resource_info_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/gmm_tests.cpp
)

add_subdirectories()