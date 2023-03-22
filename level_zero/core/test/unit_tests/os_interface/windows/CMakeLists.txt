#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_l0_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/global_teardown_test_values.cpp
  )
endif()
