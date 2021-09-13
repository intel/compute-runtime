#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(SUPPORT_DG1 AND "${BRANCH_TYPE}" STREQUAL "")
  list(APPEND L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_memory.h
       ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_memory_dg1.cpp
  )
else()
  list(APPEND L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}mock_memory.h
       ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}test_sysman_memory.cpp
  )
endif()

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX}
  )
endif()
