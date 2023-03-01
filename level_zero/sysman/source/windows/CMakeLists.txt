#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_SYSMAN_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/os_sysman_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_sysman_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_device_id_windows.cpp
)

if(WIN32)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_SYSMAN_WINDOWS}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_SYSMAN_WINDOWS ${L0_SRCS_SYSMAN_WINDOWS})
