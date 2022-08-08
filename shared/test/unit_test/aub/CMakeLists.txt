#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_center_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper_tests.cpp
)

if(NOT DEFINED AUB_STREAM_PROJECT_NAME)
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/aub_center_using_aubstream_stubs_tests.cpp)
endif()

add_subdirectories()
