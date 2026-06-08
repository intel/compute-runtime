#
# Copyright (C) 2021-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_debug_api_windows.cpp
  )
  target_sources(${TARGET_NAME_L0}_fixtures PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_fixtures_windows.cpp
  )
endif()
