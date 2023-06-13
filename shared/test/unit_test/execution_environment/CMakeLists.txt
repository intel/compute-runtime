#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/execution_environment_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/execution_environment_tests.h
               ${CMAKE_CURRENT_SOURCE_DIR}/execution_environment_tests_helper_${DRIVER_MODEL}.cpp
)
