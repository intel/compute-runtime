#
# Copyright (C) 2023-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_TESTS_LINUX_XE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_xe_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ccs_mode_xe_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_xe_perf_tests.cpp
)
if(NEO_ENABLE_XE_EU_DEBUG_SUPPORT)
  list(APPEND NEO_CORE_OS_INTERFACE_TESTS_LINUX_XE
       ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_xe_debugger_tests.cpp
  )
endif()

set_property(GLOBAL APPEND PROPERTY NEO_CORE_OS_INTERFACE_TESTS_LINUX ${NEO_CORE_OS_INTERFACE_TESTS_LINUX_XE})

if(UNIX)
  target_sources(neo_shared_tests PRIVATE ${NEO_CORE_OS_INTERFACE_TESTS_LINUX_XE})
endif()

add_subdirectories()
