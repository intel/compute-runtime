#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_SYSMAN_FREQUENCY_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_frequency_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_frequency_imp.h
)

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_SYSMAN_FREQUENCY_LINUX}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_SYSMAN_FREQUENCY_LINUX ${L0_SRCS_SYSMAN_FREQUENCY_LINUX})
