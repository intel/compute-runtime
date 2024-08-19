#
# Copyright (C) 2022-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(NEO_ENABLE_XE_EU_DEBUG_SUPPORT)
    target_sources(${TARGET_NAME} PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_debug_api_linux_xe.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_fixtures_linux_xe.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_fixtures_linux_xe.cpp
    )
  endif()

endif()
add_subdirectories()
