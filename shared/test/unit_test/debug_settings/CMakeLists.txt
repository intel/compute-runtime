#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT SKIP_NEO_UNIT_TESTS AND NOT SKIP_UNIT_TESTS)

  target_sources(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_manager_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_manager_tests.cpp
  )

endif()
