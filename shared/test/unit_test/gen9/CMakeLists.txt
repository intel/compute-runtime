#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN9)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_cmds_gen9_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_gen9.cpp
  )
  add_subdirectories()
endif()
