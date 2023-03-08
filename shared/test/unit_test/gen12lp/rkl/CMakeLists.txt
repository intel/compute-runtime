#
# Copyright (C) 2022-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_RKL)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/excludes_gen12lp_rkl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_gfx_core_helper_rkl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_product_helper_rkl.cpp
  )

  add_subdirectories()
endif()
