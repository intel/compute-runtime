#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_SYSMAN_ENGINE_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_engine_imp.h
)

if(NEO_ENABLE_i915_PRELIM_DETECTION)
  list(APPEND L0_SRCS_SYSMAN_ENGINE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_engine_imp_prelim.cpp
  )
else()
  list(APPEND L0_SRCS_SYSMAN_ENGINE_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_engine_imp.cpp
  )
endif()

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_SYSMAN_ENGINE_LINUX}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_SYSMAN_ENGINE_LINUX ${L0_SRCS_SYSMAN_ENGINE_LINUX})
