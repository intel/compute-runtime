#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(LIBGENL_FOUND)
  set(L0_SRCS_TOOLS_SYSMAN_LINUX_NL_API
      ${CMAKE_CURRENT_SOURCE_DIR}/nl_api.h
      ${CMAKE_CURRENT_SOURCE_DIR}/nl_api.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/iaf_nl_api.h
      ${CMAKE_CURRENT_SOURCE_DIR}/iaf_nl_api.cpp
  )
endif()

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_SYSMAN_LINUX_NL_API}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_LINUX_NL_API ${L0_SRCS_TOOLS_SYSMAN_LINUX_NL_API})
