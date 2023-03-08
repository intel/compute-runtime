#
# Copyright (C) 2022-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_EHL)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/excludes_gen11_ehl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_product_helper_ehl.cpp
  )
  add_subdirectories()
endif()
