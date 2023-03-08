#
# Copyright (C) 2022-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_BXT)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_tests_bxt.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_product_helper_bxt.cpp
  )

  add_subdirectories()
endif()
