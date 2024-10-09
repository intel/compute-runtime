#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(NEO_ENABLE_I915_PRELIM_DETECTION)
  list(APPEND L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_memory_prelim.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_memory_prelim.h
  )

else()
  if(SUPPORT_DG1)
    list(APPEND L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX
         ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_memory_dg1.cpp
         ${CMAKE_CURRENT_SOURCE_DIR}/mock_memory.h
    )
  else()
    list(APPEND L0_TESTS_TOOLS_SYSMAN_MEMORY_LINUX
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
