#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_fixtures
  # local files
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt

  # necessary dependencies from igdrcl_tests
  ${NEO_RUNTIME_TESTS_DIR}/fixtures/image_fixture.cpp
  ${NEO_RUNTIME_TESTS_DIR}/fixtures/platform_fixture.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_fixtures})
