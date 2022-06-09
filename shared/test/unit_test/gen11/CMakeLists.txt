#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN11)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/coherency_tests_gen11.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_cmds_gen11_tests.cpp
  )
  add_subdirectories()
endif()
