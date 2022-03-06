#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_RAS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}os_ras_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}os_ras_imp.h
)

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_SYSMAN_RAS_LINUX}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_RAS_LINUX ${L0_SRCS_TOOLS_SYSMAN_RAS_LINUX})
add_subdirectories()

