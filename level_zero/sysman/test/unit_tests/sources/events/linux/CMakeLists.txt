#
# Copyright (C) 2023-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_SYSMAN_EVENTS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_events.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_events.h
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_SYSMAN_EVENTS_LINUX}
  )

  if(LIBGENL_FOUND)
    target_sources(${TARGET_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_events_netlink.cpp
    )
  endif()
endif()
