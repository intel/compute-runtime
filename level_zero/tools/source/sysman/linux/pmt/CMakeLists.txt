#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_LINUX_PMT
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}pmt_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pmt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pmt.h
)
if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_SYSMAN_LINUX_PMT}
  )
endif()
# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_PMT_LINUX ${L0_SRCS_TOOLS_SYSMAN_PMT_LINUX})
