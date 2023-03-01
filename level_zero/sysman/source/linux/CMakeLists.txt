#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_SYSMAN_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/os_sysman_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_sysman_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fs_access.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fs_access.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_device_id_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_device_id_linux.h
)

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_SYSMAN_LINUX}
  )
endif()

add_subdirectories()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_SYSMAN_LINUX ${L0_SRCS_SYSMAN_LINUX})
