#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_DIRECT_SUBMISSION_WINDOWS_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_direct_submission_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_DIRECT_SUBMISSION_WINDOWS_TESTS ${NEO_CORE_DIRECT_SUBMISSION_WINDOWS_TESTS})

add_subdirectories()