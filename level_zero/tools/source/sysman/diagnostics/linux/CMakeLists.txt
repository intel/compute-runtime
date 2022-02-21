#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_DIAGNOSTICS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/os_diagnostics_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_diagnostics_imp.h
)

if(NEO_ENABLE_i915_PRELIM_DETECTION)
  list(APPEND L0_SRCS_TOOLS_SYSMAN_DIAGNOSTICS_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/os_diagnostics_helper_prelim.cpp
  )
else()
  list(APPEND L0_SRCS_TOOLS_SYSMAN_DIAGNOSTICS_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/os_diagnostics_helper.cpp
  )
endif()

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_SYSMAN_DIAGNOSTICS_LINUX}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_DIAGNOSTICS_LINUX ${L0_SRCS_TOOLS_SYSMAN_DIAGNOSTICS_LINUX})
