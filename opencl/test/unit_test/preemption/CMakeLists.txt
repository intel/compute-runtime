#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(igdrcl_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/preemption_tests.cpp
)

add_subdirectories()
