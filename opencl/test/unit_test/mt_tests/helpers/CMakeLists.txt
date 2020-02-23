#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_helpers
  # local files
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/interlocked_max_mt_tests.cpp

  # necessary dependencies from igdrcl_tests
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers/base_object_tests_mt.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_helpers})
