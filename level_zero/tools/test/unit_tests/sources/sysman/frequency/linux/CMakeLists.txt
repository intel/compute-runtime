#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_TOOLS_SYSMAN_FREQUENCY_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(NEO_ENABLE_i915_PRELIM_DETECTION)
  list(APPEND L0_TESTS_TOOLS_SYSMAN_FREQUENCY_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_frequency_prelim.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_frequency_prelim.h
  )
else()
  list(APPEND L0_TESTS_TOOLS_SYSMAN_FREQUENCY_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_frequency.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_frequency.h
  )
endif()

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_FREQUENCY_LINUX}
  )
endif()

