#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_TESTS_LINUX_XE_EUDEBUG
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)
if(NEO_ENABLE_XE_EU_DEBUG_SUPPORT)
  if(NEO_USE_XE_EU_DEBUG_EXP_UPSTREAM)
    list(APPEND NEO_CORE_OS_INTERFACE_TESTS_LINUX_XE_EUDEBUG
         ${CMAKE_CURRENT_SOURCE_DIR}/eudebug_interface_upstream_tests.cpp
    )
  endif()
  if(NEO_ENABLE_XE_PRELIM_DETECTION)
    list(APPEND NEO_CORE_OS_INTERFACE_TESTS_LINUX_XE_EUDEBUG
         ${CMAKE_CURRENT_SOURCE_DIR}/eudebug_interface_prelim_tests.cpp
         ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}eudebug_interface_prelim_tests_extra.cpp
    )
  endif()
endif()

set_property(GLOBAL APPEND PROPERTY NEO_CORE_OS_INTERFACE_TESTS_LINUX ${NEO_CORE_OS_INTERFACE_TESTS_LINUX_XE_EUDEBUG})

if(UNIX)
  target_sources(neo_shared_tests PRIVATE ${NEO_CORE_OS_INTERFACE_TESTS_LINUX_XE_EUDEBUG})
endif()

add_subdirectories()
