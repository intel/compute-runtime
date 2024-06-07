#
# Copyright (C) 2023-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_DIR_SUFFIX}product_helper_tests_extra_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests_dg2.cpp
  )
endif()

if(WIN32 OR(UNIX AND NOT DISABLE_WDDM_LINUX))
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager_tests_dg2.cpp
  )
endif()