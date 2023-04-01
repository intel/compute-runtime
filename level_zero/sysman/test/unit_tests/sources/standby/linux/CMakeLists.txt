#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_SYSMAN_STANDBY_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

list(APPEND L0_TESTS_SYSMAN_STANDBY_LINUX
     ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_sysman_standby.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_standby.h
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_SYSMAN_STANDBY_LINUX}
  )
endif()
