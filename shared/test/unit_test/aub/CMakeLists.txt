#
# Copyright (C) 2018-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_center_tbx_shared_memory_tests.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_center_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/tbx_in_process_simulator_tests.cpp
)

target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/aub_center_using_aubstream_stubs_tests.cpp)

add_subdirectories()
