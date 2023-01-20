#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN8)
  target_sources(neo_libult_common PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_gen8.h
  )

  add_subdirectories()
endif()
