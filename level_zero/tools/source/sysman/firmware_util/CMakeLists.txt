#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(igsc_FOUND)
  set(L0_SRCS_TOOLS_SYSMAN_FIRMWARE_UTIL
      ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_imp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_imp.h
      ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util.h
      ${CMAKE_CURRENT_SOURCE_DIR}/igsc_wrapper.h
      ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_imp_helper.cpp
  )
  add_subdirectories()
else()
  set(L0_SRCS_TOOLS_SYSMAN_FIRMWARE_UTIL
      ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_imp_stub.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util.h
      ${CMAKE_CURRENT_SOURCE_DIR}/igsc_wrapper.h
  )
endif()

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_TOOLS_SYSMAN_FIRMWARE_UTIL}
)

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_FIRMWARE_UTILITY ${L0_SRCS_TOOLS_SYSMAN_FIRMWARE_UTILITY})
