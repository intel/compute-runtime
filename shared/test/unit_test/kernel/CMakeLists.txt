#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_SHARED_KERNEL_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_descriptor_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_metadata_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_descriptor_from_patchtokens_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_descriptor_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_SHARED_KERNEL_TESTS ${NEO_SHARED_KERNEL_TESTS})

add_subdirectories()