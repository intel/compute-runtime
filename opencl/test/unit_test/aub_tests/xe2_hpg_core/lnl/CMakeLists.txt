#
# Copyright (C) 2024-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_LNL)
  target_sources(igdrcl_aub_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_excludes_lnl.cpp
  )
endif()
