#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_TOOLS_SYSMAN_EVENTS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_events.h
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_EVENTS_LINUX}
  )
endif()
