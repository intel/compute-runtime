#
# Copyright (C) 2021-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(ze_intel_gpu_aub_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/append_kernel_indirect_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/multitile_aub_tests.cpp
)
