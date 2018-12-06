#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_aub_helper_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_center_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper_tests.cpp
)

if(NOT DEFINED AUB_STREAM_DIR)
  list(APPEND IGDRCL_SRCS_aub_helper_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_center_using_aubstream_stubs_tests.cpp
  )
endif()

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_aub_helper_tests})
add_subdirectories()
