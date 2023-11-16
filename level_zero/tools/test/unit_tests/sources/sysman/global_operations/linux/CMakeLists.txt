#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_TOOLS_SYSMAN_GLOBAL_OPERATIONS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_global_operations.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_global_operations.h
)

if(NEO_ENABLE_i915_PRELIM_DETECTION)
  list(APPEND L0_TESTS_TOOLS_SYSMAN_GLOBAL_OPERATIONS_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_global_operations_prelim.cpp
  )
endif()

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_GLOBAL_OPERATIONS_LINUX}
  )
endif()
