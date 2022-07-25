#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_RKL)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/excludes_gen12lp_rkl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_rkl.cpp
  )

  add_subdirectories()
endif()
