#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_DEBUG_SETTINGS_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_manager_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_manager_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_DEBUG_SETTINGS_TESTS ${NEO_CORE_DEBUG_SETTINGS_TESTS})