#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_SYSMAN_FREQUENCY
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_frequency.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_frequency.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_frequency_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_frequency_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_frequency.h
)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_SYSMAN_FREQUENCY}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(UNIX)
  add_subdirectory(linux)
else()
  add_subdirectory(windows)
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_SYSMAN_FREQUENCY ${L0_SRCS_SYSMAN_FREQUENCY})
