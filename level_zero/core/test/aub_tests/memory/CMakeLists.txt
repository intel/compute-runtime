#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(ze_intel_gpu_aub_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/memory_aub_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/memory_copy_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/memory_fill_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/memory_read_write_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_aub_tests.cpp
)
