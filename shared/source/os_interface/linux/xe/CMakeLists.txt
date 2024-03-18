#
# Copyright (C) 2023-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NEO_ENABLE_XE_DRM_DETECTION)
  set(NEO_CORE_OS_INTERFACE_LINUX_XE
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/drm_version_xe.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_xe.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_xe_debugger.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}ioctl_helper_xe_string_value_getter.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}ioctl_helper_xe_vm_export.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}ioctl_helper_xe_perf.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_xe.h
  )

  set_property(GLOBAL APPEND PROPERTY NEO_CORE_OS_INTERFACE_LINUX ${NEO_CORE_OS_INTERFACE_LINUX_XE})
endif()
