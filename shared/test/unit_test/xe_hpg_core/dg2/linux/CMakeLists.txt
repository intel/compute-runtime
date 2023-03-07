#
# Copyright (C) 2022-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests_dg2.cpp
  )
  add_subdirectories()
endif()
