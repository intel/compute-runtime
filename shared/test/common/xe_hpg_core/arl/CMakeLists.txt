#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ARL)
  target_sources(neo_libult_common PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_configs_arl.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_arl.h
  )
  add_subdirectories()
endif()
