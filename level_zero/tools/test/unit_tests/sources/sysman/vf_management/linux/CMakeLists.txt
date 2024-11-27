#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_SYSMAN_VF_MANAGEMENT_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

list(APPEND L0_TESTS_SYSMAN_VF_MANAGEMENT_LINUX
     ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_sysman_vf_management.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_sysman_vf_management_prelim.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_vf_management.h
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_SYSMAN_VF_MANAGEMENT_LINUX}
  )
endif()
