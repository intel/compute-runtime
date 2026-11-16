#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XEHP_AND_LATER)
  target_sources(ze_intel_gpu_aub_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_memory_ranges_barrier_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_multicontext_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_one_va_multi_physical_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_postsync_write_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/compression_aub_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/multi_tile_buffers_aub_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/multicontext_cmdqueue_l0_aub_fixture.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/multicontext_cmdqueue_l0_aub_fixture.h
  )
endif()
