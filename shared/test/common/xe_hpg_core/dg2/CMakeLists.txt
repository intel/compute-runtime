#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_DG2)
  target_sources(neo_libult_common PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_configs_dg2.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_dg2.h
  )

  add_subdirectories()
endif()