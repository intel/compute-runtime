#
# Copyright (C) 2023-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_LINUX_XE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}create_ioctl_helper_xe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_xe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_xe.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_xe_perf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/xedrm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/xedrm_prelim.h
)

if(NEO_ENABLE_XE_EU_DEBUG_SUPPORT)
  list(APPEND NEO_CORE_OS_INTERFACE_LINUX_XE
       ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_xe_debugger.cpp
  )
else()
  list(APPEND NEO_CORE_OS_INTERFACE_LINUX_XE
       ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_xe_debugger_stubs.cpp
  )
endif()

set_property(GLOBAL APPEND PROPERTY NEO_CORE_OS_INTERFACE_LINUX ${NEO_CORE_OS_INTERFACE_LINUX_XE})
add_subdirectories()
