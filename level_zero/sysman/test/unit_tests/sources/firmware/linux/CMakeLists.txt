#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_SYSMAN_FIRMWARE_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_sysman_firmware.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_zes_sysman_firmware.h
    ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_DIR_SUFFIX}test_zes_sysman_firmware_extended.cpp
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_SYSMAN_FIRMWARE_LINUX}
  )
endif()

add_subdirectories()