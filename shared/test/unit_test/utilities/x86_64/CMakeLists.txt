#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(${NEO_TARGET_PROCESSOR} STREQUAL "x86_64")
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/cpuinfo_tests_x86_64.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wait_util_tests_x86_64.cpp
  )

  add_subdirectories()
endif()