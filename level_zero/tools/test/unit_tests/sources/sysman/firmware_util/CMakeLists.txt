#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(igsc_FOUND)
  set(L0_SRCS_TOOLS_SYSMAN_LINUX_FIRMWARE_UTIL_TEST
      ${CMAKE_CURRENT_SOURCE_DIR}/test_fw_util.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/mock_fw_util_fixture.h
      ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}test_fw_util_helper.cpp
  )
endif()

target_sources(${TARGET_NAME} PRIVATE
               ${L0_SRCS_TOOLS_SYSMAN_LINUX_FIRMWARE_UTIL_TEST}
)

