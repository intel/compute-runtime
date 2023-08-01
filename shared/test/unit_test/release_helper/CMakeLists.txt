#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)
if(TESTS_XE_HPG_CORE)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_12_55_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_12_56_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_12_57_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_12_70_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_12_71_tests.cpp
  )
endif()
add_subdirectories()