#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_tests_nvlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests_nvlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_id_tests_nvlp.cpp
  )
endif()
