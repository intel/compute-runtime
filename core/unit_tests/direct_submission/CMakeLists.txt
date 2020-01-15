#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_DIRECT_SUBMISSION_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_DIRECT_SUBMISSION_TESTS ${NEO_CORE_DIRECT_SUBMISSION_TESTS})

add_subdirectories()