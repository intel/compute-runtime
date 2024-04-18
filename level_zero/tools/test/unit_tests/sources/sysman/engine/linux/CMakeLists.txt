#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_TOOLS_SYSMAN_ENGINE_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_engine_prelim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_engine_prelim.h
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_engine.h
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_ENGINE_LINUX}
  )
endif()
