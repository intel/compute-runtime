#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(igsc_FOUND)
  set(L0_SRCS_TOOLS_SYSMAN_LINUX_FIRMWARE_UTIL_TEST
      ${CMAKE_CURRENT_SOURCE_DIR}/test_fw_util.cpp
  )
endif()

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_fixture.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_procfs_access_fixture.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_access_fixture.h
                 ${L0_SRCS_TOOLS_SYSMAN_LINUX_FIRMWARE_UTIL_TEST}
  )
endif()

add_subdirectories()
