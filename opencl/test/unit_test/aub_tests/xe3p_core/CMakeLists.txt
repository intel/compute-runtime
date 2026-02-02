#
# Copyright (C) 2025-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE3P_CORE)
  target_sources(igdrcl_aub_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel_aub_tests_xe3p_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/system_memfence_aub_tests_xe3p_core.cpp
  )
  add_subdirectories()
endif()
