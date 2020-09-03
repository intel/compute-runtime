#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_DG1)
  set(L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/dg1/mock_memory.h
      ${CMAKE_CURRENT_SOURCE_DIR}/dg1${BRANCH_DIR_SUFFIX}/test_sysman_memory.cpp
  )
else()
  set(L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_memory.cpp
  )
endif()

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX}
  )
endif()
