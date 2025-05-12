#
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPG_CORE)
  target_sources(igdrcl_aub_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump_tests_xe_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_test_excludes_xe_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_stateless_compression_in_sba_xe_hpg_core.cpp
  )
  add_subdirectories()
endif()
