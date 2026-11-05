#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(ze_intel_gpu_aub_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_mi_atomic_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_aub_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_aub_fixture.h
)

if(TESTS_XEHP_AND_LATER)
  target_sources(ze_intel_gpu_aub_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_range_based_flush_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_walker_partition_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/copy_engine_aub_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/copy_engine_aub_tests_xehp_and_later.h
  )
endif()

if(TESTS_DG2_AND_LATER)
  target_sources(ze_intel_gpu_aub_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/mi_math_aub_tests_dg2_and_later.cpp
  )
endif()

if(TESTS_XE3P_AND_LATER)
  target_sources(ze_intel_gpu_aub_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_tests_xe3p_and_later.cpp
  )
endif()

if(TESTS_XE_HPC_CORE)
  target_sources(ze_intel_gpu_aub_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/copy_engine_aub_tests_xe_hpc_core.cpp
  )
endif()
