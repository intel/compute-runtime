#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_debug_session_linux_create.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_fixtures_linux.cpp
  )
endif()

add_subdirectories()
