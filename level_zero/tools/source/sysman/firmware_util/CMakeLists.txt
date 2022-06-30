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
  )
  if(igsc_VERSION VERSION_LESS 0.3)
    message(STATUS "default igsc version: ${igsc_VERSION}")
    set(L0_SRCS_TOOLS_SYSMAN_FIRMWARE_UTIL_HELPER
        ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_imp_helper.cpp
    )
  else()
    message(STATUS "IGSC version: ${igsc_VERSION}")
    set(L0_SRCS_TOOLS_SYSMAN_FIRMWARE_UTIL_HELPER
        ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}firmware_util_imp_helper.cpp
    )
  endif()
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
               ${L0_SRCS_TOOLS_SYSMAN_FIRMWARE_UTIL_HELPER}
)

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_FIRMWARE_UTILITY ${L0_SRCS_TOOLS_SYSMAN_FIRMWARE_UTILITY})
