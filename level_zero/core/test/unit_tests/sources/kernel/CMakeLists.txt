#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/test_function.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_kernel.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_sampler_patch_value.cpp
)
