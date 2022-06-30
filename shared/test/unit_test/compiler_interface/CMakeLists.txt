#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/compiler_cache_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/compiler_interface_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/compiler_options_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/external_functions_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/intermediate_representations_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/linker_mock.h
               ${CMAKE_CURRENT_SOURCE_DIR}/linker_tests.cpp
)

