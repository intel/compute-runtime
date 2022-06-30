#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/gmm_callbacks_tests_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests_gen12lp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_tests_gen12lp.cpp
  )
  add_subdirectories()
endif()
