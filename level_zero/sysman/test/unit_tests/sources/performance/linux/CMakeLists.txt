#
# Copyright (C) 2023-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_SYSMAN_PERFORMANCE_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_performance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_performance.h
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_performance_prelim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_performance_prelim.h
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_SYSMAN_PERFORMANCE_LINUX}
  )
endif()
