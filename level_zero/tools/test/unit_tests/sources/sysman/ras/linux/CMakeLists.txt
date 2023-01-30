#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_TOOLS_SYSMAN_RAS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(NEO_ENABLE_i915_PRELIM_DETECTION)
  list(APPEND L0_TESTS_TOOLS_SYSMAN_RAS_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_ras_prelim.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_fs_ras_prelim.h
  )
else()
  list(APPEND L0_TESTS_TOOLS_SYSMAN_RAS_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_ras.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_fs_ras.h
  )
endif()

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_RAS_LINUX}
  )
endif()

