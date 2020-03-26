#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_DIRECT_SUBMISSION_DISPATCHERS_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/blitter_dispatcher_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatcher_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatcher_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatcher_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/render_dispatcher_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_DIRECT_SUBMISSION_DISPATCHERS_TESTS ${NEO_CORE_DIRECT_SUBMISSION_DISPATCHERS_TESTS})
add_subdirectories()