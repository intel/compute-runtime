#
# Copyright (C) 2022-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(NEO_ENABLE_I915_PRELIM_DETECTION)
    target_sources(${TARGET_NAME} PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_fixtures_linux.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_fixtures_linux.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_debug_api_linux.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/tile_debug_session_linux_tests.cpp
    )
  endif()
endif()
