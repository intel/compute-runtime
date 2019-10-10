#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_mt_tests_fixtures
  # local files
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt

  # necessary dependencies from igdrcl_tests
  ${NEO_SOURCE_DIR}/unit_tests/fixtures/image_fixture.cpp
  ${NEO_SOURCE_DIR}/unit_tests/fixtures/platform_fixture.cpp
)
target_sources(igdrcl_mt_tests PRIVATE ${IGDRCL_SRCS_mt_tests_fixtures})
