#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests_cri.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_id_tests_cri.cpp
  )
endif()
