#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_DG1)
  set(L0_SRCS_TOOLS_SYSMAN_MEMORY_LINUX
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/dg1${BRANCH_DIR_SUFFIX}/os_memory_imp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/dg1/os_memory_imp.h
  )
else()
  set(L0_SRCS_TOOLS_SYSMAN_MEMORY_LINUX
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_imp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/os_memory_imp.h
  )
endif()

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_SYSMAN_MEMORY_LINUX}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_MEMORY_LINUX ${L0_SRCS_TOOLS_SYSMAN_MEMORY_LINUX})
