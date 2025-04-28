#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_SYSMAN_POWER_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_power.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_power_xe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_power_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_power.h
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_SYSMAN_POWER_LINUX}
  )
endif()
