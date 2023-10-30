#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SYSMAN_SHARED_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_kmd_interface_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_kmd_interface_tests.h
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_SYSMAN_SHARED_TESTS}
  )
endif()

