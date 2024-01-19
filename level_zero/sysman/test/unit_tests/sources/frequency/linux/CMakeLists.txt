#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_SYSMAN_FREQUENCY_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_frequency.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_frequency.h
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_frequency_xe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_frequency_xe.h
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_SYSMAN_FREQUENCY_LINUX}
  )
endif()

add_subdirectories()
