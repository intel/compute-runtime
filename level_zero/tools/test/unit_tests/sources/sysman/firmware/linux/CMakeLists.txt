#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_TOOLS_SYSMAN_FIRMWARE_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(NEO_ENABLE_I915_PRELIM_DETECTION)
  list(APPEND L0_TESTS_TOOLS_SYSMAN_FIRMWARE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_sysman_firmware_prelim.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_zes_sysman_firmware_prelim.h
  )

else()
  list(APPEND L0_TESTS_TOOLS_SYSMAN_FIRMWARE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_sysman_firmware.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_zes_sysman_firmware.h
  )
endif()

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_FIRMWARE_LINUX}
  )
endif()
