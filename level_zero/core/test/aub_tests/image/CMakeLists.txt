#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(ze_intel_gpu_aub_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/image_aub_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/image_copy_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/image_copy_from_memory_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/image_copy_to_memory_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/image_create_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/image_fill_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/image_verify_memory_aub_tests.cpp
)
