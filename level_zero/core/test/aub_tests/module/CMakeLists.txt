#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(ze_intel_gpu_aub_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/variable_register_per_thread_aub_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/variable_register_per_thread_aub_tests.cpp
)
