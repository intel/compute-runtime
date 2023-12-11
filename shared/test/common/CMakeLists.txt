#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

include(${NEO_SOURCE_DIR}/cmake/setup_ult_global_flags.cmake)

add_library(neo_libult_common OBJECT EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/tests_configuration.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests_configuration.h
)

add_subdirectories()
