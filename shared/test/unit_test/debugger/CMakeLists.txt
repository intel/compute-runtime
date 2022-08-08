#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_l0_debugger.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_l0_debugger_single_address_space.cpp
)
