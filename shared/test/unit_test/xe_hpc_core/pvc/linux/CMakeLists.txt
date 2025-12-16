#
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_id_tests_pvc.cpp
  )
  add_subdirectories()
endif()
