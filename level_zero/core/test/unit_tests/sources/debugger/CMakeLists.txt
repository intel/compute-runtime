#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/active_debugger_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/l0_debugger_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/test_source_level_debugger.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_l0_debugger_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_l0_debugger_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_module_with_debug.cpp
)

add_subdirectories()
