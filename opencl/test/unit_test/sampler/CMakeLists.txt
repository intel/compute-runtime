#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_sampler
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/get_sampler_info_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_set_arg_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests.cpp
)

if(TESTS_XEHP_AND_LATER)
  list(APPEND IGDRCL_SRCS_tests_sampler
       ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests_xehp_and_later.cpp
  )
endif()

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_sampler})
add_subdirectories()
