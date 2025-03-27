#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/printf_helper_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/program_info_from_patchtokens_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/program_info_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/program_initialization_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/metadata_generation_tests.cpp
)
