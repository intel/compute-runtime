#
# Copyright (C) 2022-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(igsc_FOUND)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_fw_util_fixture.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_fw_util.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_fw_util_helper.cpp
  )
endif()

add_subdirectories()

