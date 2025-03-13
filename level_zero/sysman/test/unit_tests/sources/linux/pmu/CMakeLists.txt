#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_pmu.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_pmu_xe.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_pmu.h
  )
endif()

add_subdirectories()
