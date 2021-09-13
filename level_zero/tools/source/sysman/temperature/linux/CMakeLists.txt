#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_TEMPERATURE_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}os_temperature_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_temperature_imp.h
)

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_SYSMAN_TEMPERATURE_LINUX}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_TEMPERATURE_LINUX ${L0_SRCS_TOOLS_SYSMAN_TEMPERATURE_LINUX})
