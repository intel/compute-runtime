#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_DG1)
  set(L0_TESTS_TOOLS_SYSMAN_POWER_LINUX
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/dg1/test_zes_power.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/dg1/mock_sysfs_power.h
  )
else()
  set(L0_TESTS_TOOLS_SYSMAN_POWER_LINUX
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_power.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_power.h
  )
endif()

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_POWER_LINUX}
  )
endif()
