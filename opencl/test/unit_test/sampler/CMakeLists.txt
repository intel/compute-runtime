#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_sampler
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/get_sampler_info_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sampler_set_arg_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_sampler})
add_subdirectories()
