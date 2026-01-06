#
# Copyright (C) 2023-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests_arl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_id_tests_arl.cpp
  )
  add_subdirectories()
endif()
