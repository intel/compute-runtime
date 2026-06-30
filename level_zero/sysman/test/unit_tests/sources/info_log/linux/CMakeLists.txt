#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_SYSMAN_INFO_LOG_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_sysman_info_log.cpp
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_SYSMAN_INFO_LOG_LINUX}
  )
endif()