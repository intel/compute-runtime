#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_TOOLS_SYSMAN_POWER_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_power.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_power.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/mock_sysfs_power_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/test_zes_power_helper.cpp
)

if((NEO_ENABLE_i915_PRELIM_DETECTION) AND ("${BRANCH_TYPE}" STREQUAL ""))
  list(REMOVE_ITEM L0_TESTS_TOOLS_SYSMAN_POWER_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_power_helper.h
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_power_helper.cpp
  )
endif()

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_POWER_LINUX}
  )
endif()
