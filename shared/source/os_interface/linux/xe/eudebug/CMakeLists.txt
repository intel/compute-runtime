#
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_LINUX_XE_EUDEBUG
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/create_eudebug_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/eudebug_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/eudebug_wrappers.h
)
if(NEO_ENABLE_XE_EU_DEBUG_SUPPORT)
  if(NEO_USE_XE_EU_DEBUG_EXP_UPSTREAM)
    list(APPEND NEO_CORE_OS_INTERFACE_LINUX_XE_EUDEBUG
         ${CMAKE_CURRENT_SOURCE_DIR}/eudebug_interface_upstream.cpp
         ${CMAKE_CURRENT_SOURCE_DIR}/eudebug_interface_upstream.h
    )
  endif()
  if(NEO_ENABLE_XE_PRELIM_DETECTION)
    list(APPEND NEO_CORE_OS_INTERFACE_LINUX_XE_EUDEBUG
         ${CMAKE_CURRENT_SOURCE_DIR}/eudebug_interface_prelim.h
         ${CMAKE_CURRENT_SOURCE_DIR}/eudebug_interface_prelim.cpp
         ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}eudebug_interface_prelim_extra.cpp
    )
  endif()
endif()

set_property(GLOBAL APPEND PROPERTY NEO_CORE_OS_INTERFACE_LINUX ${NEO_CORE_OS_INTERFACE_LINUX_XE_EUDEBUG})
add_subdirectories()
