#
# Copyright (C) 2023-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_SYSMAN_RAS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_ras.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_ras_exp.cpp
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_SYSMAN_RAS_LINUX}
  )

  if(LIBGENL_FOUND)
    target_sources(${TARGET_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_ras_netlink.cpp
    )
  endif()
endif()
