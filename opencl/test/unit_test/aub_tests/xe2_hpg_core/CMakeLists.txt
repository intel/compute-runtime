#
# Copyright (C) 2024-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE2_HPG_CORE)
  target_sources(igdrcl_aub_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  )
  add_subdirectories()
endif()
