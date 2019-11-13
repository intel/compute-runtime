#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(igdrcl_aub_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_aub_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_copy_aub_tests.cpp
)
