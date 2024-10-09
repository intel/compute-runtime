#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_TOOLS_SYSMAN_SCHEDULER_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(NEO_ENABLE_I915_PRELIM_DETECTION)
  list(APPEND L0_TESTS_TOOLS_SYSMAN_SCHEDULER_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_scheduler_prelim.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_scheduler_prelim.h
  )

else()
  list(APPEND L0_TESTS_TOOLS_SYSMAN_SCHEDULER_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_scheduler.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_scheduler.h
  )
endif()

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_SCHEDULER_LINUX}
  )
endif()
