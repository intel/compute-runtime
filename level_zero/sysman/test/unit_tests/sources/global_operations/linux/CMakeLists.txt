#
# Copyright (C) 2023-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_SYSMAN_GLOBAL_OPERATIONS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_global_operations.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_global_operations_xe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_global_operations_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_global_operations.h
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_SYSMAN_GLOBAL_OPERATIONS_LINUX}
  )
endif()
