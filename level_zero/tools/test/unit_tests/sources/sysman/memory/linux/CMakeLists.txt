#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

list(APPEND L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX
     ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_memory.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/mock_memory.h
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX}
  )
endif()
