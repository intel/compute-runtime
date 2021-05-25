#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_SHARED_aub_tests_configurations
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration.inl
)

add_subdirectories()
set_property(GLOBAL PROPERTY NEO_SHARED_aub_tests_configurations ${NEO_SHARED_aub_tests_configurations})
