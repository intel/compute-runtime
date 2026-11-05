#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(ze_intel_gpu_aub_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/kernel_aub_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/printf_kernel_aub_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/system_memfence_aub_tests.cpp
)
if(TESTS_XEHP_AND_LATER)
  target_sources(ze_intel_gpu_aub_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/inline_data_local_id_aub_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/large_grf_aub_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/scratch_space_aub_tests_xehp_and_later.cpp
  )
endif()
