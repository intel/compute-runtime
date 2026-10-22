#
# Copyright (C) 2022-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests_gen12lp.cpp
  )
endif()

if(WIN32 OR(UNIX AND NOT DISABLE_WDDM_LINUX))
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_tests_gen12lp.cpp
  )
endif()
