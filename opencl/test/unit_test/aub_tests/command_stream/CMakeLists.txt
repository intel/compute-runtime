#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(igdrcl_aub_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump_tests.h
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_mi_atomic_tests.cpp
)

if(TESTS_XEHP_AND_LATER)
  target_sources(igdrcl_aub_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_range_based_flush_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_walker_partition_tests_xehp_and_later.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/copy_engine_aub_tests_xehp_and_later.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/copy_engine_aub_tests_xehp_and_later.cpp
  )
endif()

if(TESTS_DG2_AND_LATER)
  target_sources(igdrcl_aub_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/mi_math_aub_tests_dg2_and_later.cpp
  )
endif()

add_subdirectories()
