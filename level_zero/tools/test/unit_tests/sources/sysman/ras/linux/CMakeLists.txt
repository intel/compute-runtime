#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_TOOLS_SYSMAN_RAS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_ras.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_ras.h
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_ras_exp.cpp
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_RAS_LINUX}
  )
endif()

