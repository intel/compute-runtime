#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NEO_ENABLE_XE_DRM_DETECTION)
  set(NEO_CORE_OS_INTERFACE_TESTS_LINUX_XE
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_xe_tests.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_xe_vm_export_tests.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/ioctl_helper_xe_tests.h
  )
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_OS_INTERFACE_TESTS_LINUX ${NEO_CORE_OS_INTERFACE_TESTS_LINUX_XE})

  if(UNIX)
    target_sources(neo_shared_tests PRIVATE ${NEO_CORE_OS_INTERFACE_TESTS_LINUX_XE})
  endif()

  add_subdirectories()
endif()
