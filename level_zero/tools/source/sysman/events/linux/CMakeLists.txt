#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_EVENTS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(NEO_ENABLE_i915_PRELIM_DETECTION)
  list(APPEND L0_SRCS_TOOLS_SYSMAN_EVENTS_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/os_events_imp_prelim.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/os_events_imp_prelim.h
  )
else()
  list(APPEND L0_SRCS_TOOLS_SYSMAN_EVENTS_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/os_events_imp.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/os_events_imp.h
  )
endif()

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_SYSMAN_EVENTS_LINUX}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_EVENTS_LINUX ${L0_SRCS_TOOLS_SYSMAN_EVENTS_LINUX})
