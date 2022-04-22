#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/implicit_args_helper_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_descriptor_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_metadata_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/kernel_descriptor_from_patchtokens_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/kernel_descriptor_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/kernel_raytracing_tests.cpp
)

add_subdirectories()
