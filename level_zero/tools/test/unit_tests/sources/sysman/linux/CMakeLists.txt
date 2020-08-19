#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_fixture.h
  )
endif()

add_subdirectories()
