#
# Copyright (C) 2020-2022 Intel Corporation
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

if((NEO_ENABLE_i915_PRELIM_DETECTION) AND ("${BRANCH_TYPE}" STREQUAL ""))
  if(SUPPORT_DG1)
    list(REMOVE_ITEM L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX
         ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_memory_dg1.cpp
         ${CMAKE_CURRENT_SOURCE_DIR}/mock_memory.h
    )
  else()
    list(REMOVE_ITEM L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX
         ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_memory.cpp
         ${CMAKE_CURRENT_SOURCE_DIR}/mock_memory.h
    )
  endif()
endif()

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX}
  )
endif()
