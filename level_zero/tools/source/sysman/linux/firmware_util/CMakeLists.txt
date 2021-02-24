#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(libigsc_FOUND)
  set(L0_SRCS_TOOLS_SYSMAN_LINUX_FIRMWARE_UTIL
      ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/firmware_util_imp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/firmware_util_imp.h
      ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_base.h
      ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_extended.h
      ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util.h
  )

else()
  set(L0_SRCS_TOOLS_SYSMAN_LINUX_FIRMWARE_UTIL
      ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_imp_stub.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_base.h
      ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_extended.h
      ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util.h
  )
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/firmware_util_extended.h ${CMAKE_CURRENT_SOURCE_DIR}/firmware_util_extended.h)

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_SYSMAN_LINUX_FIRMWARE_UTIL}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_FIRMWARE_UTIL_LINUX ${L0_SRCS_TOOLS_SYSMAN_FIRMWARE_UTIL_LINUX})
