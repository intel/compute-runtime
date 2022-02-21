#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_TEMPERATURE_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/os_temperature_imp.h
)

if(NEO_ENABLE_i915_PRELIM_DETECTION)
  list(APPEND L0_SRCS_TOOLS_SYSMAN_TEMPERATURE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/os_temperature_imp_prelim.cpp
  )
else()
  list(APPEND L0_SRCS_TOOLS_SYSMAN_TEMPERATURE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/os_temperature_imp.cpp
  )
endif()

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_SYSMAN_TEMPERATURE_LINUX}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_TEMPERATURE_LINUX ${L0_SRCS_TOOLS_SYSMAN_TEMPERATURE_LINUX})
